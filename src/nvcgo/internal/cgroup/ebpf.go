/*
 * Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// The implementation of the device filter eBPF program in this file is based on:
// https://github.com/containers/crun/blob/0.10.2/src/libcrun/ebpf.c
//
// Although ebpf.c is originally licensed under LGPL-3.0-or-later, the author (Giuseppe Scrivano)
// agreed to relicense the file in Apache License 2.0: https://github.com/opencontainers/runc/issues/2144#issuecomment-543116397
//
// Much of the go code in this file is borrowed heavily from the (Apache licensed) file found here:
// https://github.com/opencontainers/runc/blob/8e7ab26104352f4214ab1daec5c1d4bf75eddb54/libcontainer/cgroups/ebpf/devicefilter/devicefilter.go

package cgroup

import (
	"errors"
	"fmt"
	"math"
	"os"
	"runtime"
	"unsafe"

	"github.com/cilium/ebpf"
	"github.com/cilium/ebpf/asm"
	"github.com/cilium/ebpf/link"
	"github.com/google/uuid"
	"github.com/opencontainers/runtime-spec/specs-go"
	"github.com/sirupsen/logrus"
	"golang.org/x/sys/unix"
)

func nilCloser() error {
	return nil
}

type program struct {
	insts       asm.Instructions
	hasWildCard bool
	blockID     int
}

func (p *program) init() {
	// struct bpf_cgroup_dev_ctx: https://elixir.bootlin.com/linux/v5.3.6/source/include/uapi/linux/bpf.h#L3423
	/*
		u32 access_type
		u32 major
		u32 minor
	*/
	// R2 <- type (lower 16 bit of u32 access_type at R1[0])
	p.insts = append(p.insts,
		asm.LoadMem(asm.R2, asm.R1, 0, asm.Half))

	// R3 <- access (upper 16 bit of u32 access_type at R1[0])
	p.insts = append(p.insts,
		asm.LoadMem(asm.R3, asm.R1, 0, asm.Word),
		// RSh: bitwise shift right
		asm.RSh.Imm32(asm.R3, 16))

	// R4 <- major (u32 major at R1[4])
	p.insts = append(p.insts,
		asm.LoadMem(asm.R4, asm.R1, 4, asm.Word))

	// R5 <- minor (u32 minor at R1[8])
	p.insts = append(p.insts,
		asm.LoadMem(asm.R5, asm.R1, 8, asm.Word))
}

// appendDevice needs to be called from the last element of OCI linux.resources.devices to the head element.
func (p *program) appendDevice(dev specs.LinuxDeviceCgroup, labelPrefix string) error {
	if p.blockID < 0 {
		return errors.New("the program is finalized")
	}
	if p.hasWildCard {
		// All entries after wildcard entry are ignored
		return nil
	}

	bpfType := int32(-1)
	hasType := true
	switch dev.Type {
	case string('c'):
		bpfType = int32(unix.BPF_DEVCG_DEV_CHAR)
	case string('b'):
		bpfType = int32(unix.BPF_DEVCG_DEV_BLOCK)
	case string('a'):
		hasType = false
	default:
		// if not specified in OCI json, typ is set to DeviceTypeAll
		return fmt.Errorf("invalid DeviceType %q", dev.Type)
	}
	if *dev.Major > math.MaxUint32 {
		return fmt.Errorf("invalid major %d", *dev.Major)
	}
	if *dev.Minor > math.MaxUint32 {
		return fmt.Errorf("invalid minor %d", *dev.Major)
	}
	hasMajor := *dev.Major >= 0 // if not specified in OCI json, major is set to -1
	hasMinor := *dev.Minor >= 0
	bpfAccess := int32(0)
	for _, r := range dev.Access {
		switch r {
		case 'r':
			bpfAccess |= unix.BPF_DEVCG_ACC_READ
		case 'w':
			bpfAccess |= unix.BPF_DEVCG_ACC_WRITE
		case 'm':
			bpfAccess |= unix.BPF_DEVCG_ACC_MKNOD
		default:
			return fmt.Errorf("unknown device access %v", r)
		}
	}
	// If the access is rwm, skip the check.
	hasAccess := bpfAccess != (unix.BPF_DEVCG_ACC_READ | unix.BPF_DEVCG_ACC_WRITE | unix.BPF_DEVCG_ACC_MKNOD)

	blockSym := fmt.Sprintf("%s-block-%d", labelPrefix, p.blockID)
	nextBlockSym := fmt.Sprintf("%s-block-%d", labelPrefix, p.blockID+1)
	prevBlockLastIdx := len(p.insts) - 1
	if hasType {
		p.insts = append(p.insts,
			// if (R2 != bpfType) goto next
			asm.JNE.Imm(asm.R2, bpfType, nextBlockSym),
		)
	}
	if hasAccess {
		p.insts = append(p.insts,
			// if (R3 & bpfAccess != R3 /* use R6 as a temp var */) goto next
			asm.Mov.Reg32(asm.R6, asm.R3),
			asm.And.Imm32(asm.R6, bpfAccess),
			asm.JNE.Reg32(asm.R6, asm.R3, nextBlockSym),
		)
	}
	if hasMajor {
		p.insts = append(p.insts,
			// if (R4 != major) goto next
			asm.JNE.Imm(asm.R4, int32(*dev.Major), nextBlockSym),
		)
	}
	if hasMinor {
		p.insts = append(p.insts,
			// if (R5 != minor) goto next
			asm.JNE.Imm(asm.R5, int32(*dev.Minor), nextBlockSym),
		)
	}
	if !hasType && !hasAccess && !hasMajor && !hasMinor {
		p.hasWildCard = true
	}
	p.insts = append(p.insts, p.acceptBlock(dev.Allow)...)
	// set blockSym to the first instruction we added in this iteration
	p.insts[prevBlockLastIdx+1] = p.insts[prevBlockLastIdx+1].Sym(blockSym)
	p.blockID++
	return nil
}

func (p *program) acceptBlock(accept bool) asm.Instructions {
	v := int32(0)
	if accept {
		v = 1
	}
	return []asm.Instruction{
		// R0 <- v
		asm.Mov.Imm32(asm.R0, v),
		asm.Return(),
	}
}

func (p *program) finalize(origInsts asm.Instructions, labelPrefix string) (asm.Instructions, error) {
	lenInsts := len(p.insts)
	// set blockSym to the first instruction of origInsts so we are able to jump to it properly
	blockSym := fmt.Sprintf("%s-block-%d", labelPrefix, p.blockID)
	p.insts = append(p.insts, origInsts...)
	p.insts[lenInsts] = p.insts[lenInsts].Sym(blockSym)
	p.blockID = -1
	return p.insts, nil
}

// FindAttachedCgroupDeviceFilters finds all ebpf prgrams associated with 'dirFd' that control device access
func FindAttachedCgroupDeviceFilters(dirFd int) ([]*ebpf.Program, error) {
	type bpfAttrQuery struct {
		TargetFd    uint32
		AttachType  uint32
		QueryType   uint32
		AttachFlags uint32
		ProgIds     uint64 // __aligned_u64
		ProgCnt     uint32
	}

	// Currently you can only have 64 eBPF programs attached to a cgroup.
	size := 64
	retries := 0
	for retries < 10 {
		progIds := make([]uint32, size)
		query := bpfAttrQuery{
			TargetFd:   uint32(dirFd),
			AttachType: uint32(unix.BPF_CGROUP_DEVICE),
			ProgIds:    uint64(uintptr(unsafe.Pointer(&progIds[0]))),
			ProgCnt:    uint32(len(progIds)),
		}

		// Fetch the list of program ids.
		_, _, errno := unix.Syscall(unix.SYS_BPF,
			uintptr(unix.BPF_PROG_QUERY),
			uintptr(unsafe.Pointer(&query)),
			unsafe.Sizeof(query))
		size = int(query.ProgCnt)
		runtime.KeepAlive(query)
		if errno != 0 {
			// On ENOSPC we get the correct number of programs.
			if errno == unix.ENOSPC {
				retries++
				continue
			}
			return nil, fmt.Errorf("bpf_prog_query(BPF_CGROUP_DEVICE) failed: %w", errno)
		}

		// Convert the ids to program handles.
		progIds = progIds[:size]
		programs := make([]*ebpf.Program, 0, len(progIds))
		for _, progId := range progIds {
			program, err := ebpf.NewProgramFromID(ebpf.ProgramID(progId))
			if err != nil {
				// We skip over programs that give us -EACCES or -EPERM. This
				// is necessary because there may be BPF programs that have
				// been attached (such as with --systemd-cgroup) which have an
				// LSM label that blocks us from interacting with the program.
				//
				// Because additional BPF_CGROUP_DEVICE programs only can add
				// restrictions, there's no real issue with just ignoring these
				// programs (and stops runc from breaking on distributions with
				// very strict SELinux policies).
				if errors.Is(err, os.ErrPermission) {
					logrus.Debugf("ignoring existing CGROUP_DEVICE program (prog_id=%v) which cannot be accessed by runc -- likely due to LSM policy: %v", progId, err)
					continue
				}
				return nil, fmt.Errorf("cannot fetch program from id: %w", err)
			}
			programs = append(programs, program)
		}
		runtime.KeepAlive(progIds)
		return programs, nil
	}

	return nil, errors.New("could not get complete list of CGROUP_DEVICE programs")
}

// PrependDeviceFilter prepends a set of instructions for further device filtering to an existing device filtering ebpf program
func PrependDeviceFilter(devices []specs.LinuxDeviceCgroup, origInsts asm.Instructions) (asm.Instructions, error) {
	labelPrefix := uuid.New().String()
	p := &program{}
	p.init()
	for i := len(devices) - 1; i >= 0; i-- {
		if err := p.appendDevice(devices[i], labelPrefix); err != nil {
			return nil, err
		}
	}
	insts, err := p.finalize(origInsts, labelPrefix)
	return insts, err
}

// DetachCgroupDeviceFilter detaches an existing device filter ebpf program from a cgroup.
func DetachCgroupDeviceFilter(prog *ebpf.Program, dirFd int) error {
	err := link.RawDetachProgram(link.RawDetachProgramOptions{
		Target:  dirFd,
		Program: prog,
		Attach:  ebpf.AttachCGroupDevice,
	})
	if err != nil {
		return fmt.Errorf("failed to call BPF_PROG_DETACH (BPF_CGROUP_DEVICE): %w", err)
	}
	return nil
}

// AttachCgroupDeviceFilter attaches a new device filter ebpf program to a cgroup.
func AttachCgroupDeviceFilter(prog *ebpf.Program, dirFd int) error {
	err := link.RawAttachProgram(link.RawAttachProgramOptions{
		Target:  dirFd,
		Program: prog,
		Attach:  ebpf.AttachCGroupDevice,
		Flags:   unix.BPF_F_ALLOW_MULTI,
	})
	if err != nil {
		return fmt.Errorf("failed to call BPF_PROG_ATTACH (BPF_CGROUP_DEVICE, BPF_F_ALLOW_MULTI): %w", err)
	}
	return nil
}
