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

// This file is inspired by code in the (MIT licensed) cilium eBPF package at:
// https://github.com/cilium/ebpf/blob/b38550c6f15200e3798c695890f699001b97e229/internal/syscall.go

package ebpf

import (
	"fmt"
	"unsafe"

	"golang.org/x/sys/unix"
)

type progInfo struct {
	Prog_type                uint32
	Id                       uint32
	Tag                      [unix.BPF_TAG_SIZE]byte
	Jited_prog_len           uint32
	Xlated_prog_len          uint32
	Jited_prog_insns         Pointer
	Xlated_prog_insns        Pointer
	Load_time                uint64
	Created_by_uid           uint32
	Nr_map_ids               uint32
	Map_ids                  Pointer
	Name                     [unix.BPF_OBJ_NAME_LEN]byte
	Ifindex                  uint32
	Gpl_compatible           uint32
	Netns_dev                uint64
	Netns_ino                uint64
	Nr_jited_ksyms           uint32
	Nr_jited_func_lens       uint32
	Jited_ksyms              Pointer
	Jited_func_lens          Pointer
	Btf_id                   uint32
	Func_info_rec_size       uint32
	Func_info                Pointer
	Nr_func_info             uint32
	Nr_line_info             uint32
	Line_info                Pointer
	Jited_line_info          Pointer
	Nr_jited_line_info       uint32
	Line_info_rec_size       uint32
	Jited_line_info_rec_size uint32
	Nr_prog_tags             uint32
	Prog_tags                Pointer
	Run_time_ns              uint64
	Run_cnt                  uint64
}

type progInfoLinear struct {
	Prog_type                uint32
	Id                       uint32
	Tag                      [unix.BPF_TAG_SIZE]byte
	Jited_prog_len           uint32
	Xlated_prog_len          uint32
	Jited_prog_insns         []byte
	Xlated_prog_insns        []byte
	Load_time                uint64
	Created_by_uid           uint32
	Nr_map_ids               uint32
	Map_ids                  []byte
	Name                     [unix.BPF_OBJ_NAME_LEN]byte
	Ifindex                  uint32
	Gpl_compatible           uint32
	Netns_dev                uint64
	Netns_ino                uint64
	Nr_jited_ksyms           uint32
	Nr_jited_func_lens       uint32
	Jited_ksyms              []byte
	Jited_func_lens          []byte
	Btf_id                   uint32
	Func_info_rec_size       uint32
	Func_info                []byte
	Nr_func_info             uint32
	Nr_line_info             uint32
	Line_info                []byte
	Jited_line_info          []byte
	Nr_jited_line_info       uint32
	Line_info_rec_size       uint32
	Jited_line_info_rec_size uint32
	Nr_prog_tags             uint32
	Prog_tags                []byte
	Run_time_ns              uint64
	Run_cnt                  uint64
}

type bpfObjGetInfoByFDAttr struct {
	fd      uint32
	infoLen uint32
	info    Pointer
}

// getProgramInfoLinear returns a "linear" version of the progInfo struct with all array fields filled
func getProgramInfoLinear(prog *Program) (*progInfoLinear, error) {
	var info progInfo
	attr := bpfObjGetInfoByFDAttr{
		fd:      uint32(prog.FD()),
		infoLen: uint32(unsafe.Sizeof(info)),
		info:    NewPointer(unsafe.Pointer(&info)),
	}

	_, _, errno := unix.Syscall(unix.SYS_BPF, unix.BPF_OBJ_GET_INFO_BY_FD, uintptr(unsafe.Pointer(&attr)), unsafe.Sizeof(attr))
	if errno != 0 {
		return nil, fmt.Errorf("syscall SYS_BPF:BPF_OBJ_GET_INFO_BY_FD:attr %v: %v", attr, errno)
	}

	var infoLinear progInfoLinear
	if info.Jited_prog_len > 0 {
		infoLinear.Jited_prog_insns = make([]byte, info.Jited_prog_len)
		info.Jited_prog_insns = NewSlicePointer(infoLinear.Jited_prog_insns)
	}
	if info.Xlated_prog_len > 0 {
		infoLinear.Xlated_prog_insns = make([]byte, info.Xlated_prog_len)
		info.Xlated_prog_insns = NewSlicePointer(infoLinear.Xlated_prog_insns)
	}
	if info.Nr_map_ids > 0 {
		infoLinear.Map_ids = make([]byte, info.Nr_map_ids)
		info.Map_ids = NewSlicePointer(infoLinear.Map_ids)
	}
	if info.Nr_jited_ksyms > 0 {
		infoLinear.Jited_ksyms = make([]byte, info.Nr_jited_ksyms)
		info.Jited_ksyms = NewSlicePointer(infoLinear.Jited_ksyms)
	}
	if info.Nr_jited_func_lens > 0 {
		infoLinear.Jited_func_lens = make([]byte, info.Nr_jited_func_lens)
		info.Jited_func_lens = NewSlicePointer(infoLinear.Jited_func_lens)
	}
	if info.Nr_func_info > 0 {
		infoLinear.Func_info = make([]byte, info.Nr_func_info)
		info.Func_info = NewSlicePointer(infoLinear.Func_info)
	}
	if info.Nr_line_info > 0 {
		infoLinear.Line_info = make([]byte, info.Nr_line_info)
		info.Line_info = NewSlicePointer(infoLinear.Line_info)
	}
	if info.Nr_jited_line_info > 0 {
		infoLinear.Jited_line_info = make([]byte, info.Nr_jited_line_info)
		info.Jited_line_info = NewSlicePointer(infoLinear.Jited_line_info)
	}
	if info.Nr_prog_tags > 0 {
		infoLinear.Prog_tags = make([]byte, info.Nr_prog_tags)
		info.Prog_tags = NewSlicePointer(infoLinear.Prog_tags)
	}

	_, _, errno = unix.Syscall(unix.SYS_BPF, unix.BPF_OBJ_GET_INFO_BY_FD, uintptr(unsafe.Pointer(&attr)), unsafe.Sizeof(attr))
	if errno != 0 {
		return nil, fmt.Errorf("syscall SYS_BPF:BPF_OBJ_GET_INFO_BY_FD:attr %v: %v", attr, errno)
	}

	infoLinear.Prog_type = info.Prog_type
	infoLinear.Id = info.Id
	infoLinear.Tag = info.Tag
	infoLinear.Jited_prog_len = info.Jited_prog_len
	infoLinear.Xlated_prog_len = info.Xlated_prog_len
	infoLinear.Load_time = info.Load_time
	infoLinear.Created_by_uid = info.Created_by_uid
	infoLinear.Nr_map_ids = info.Nr_map_ids
	infoLinear.Name = info.Name
	infoLinear.Ifindex = info.Ifindex
	infoLinear.Gpl_compatible = info.Gpl_compatible
	infoLinear.Netns_dev = info.Netns_dev
	infoLinear.Netns_ino = info.Netns_ino
	infoLinear.Nr_jited_ksyms = info.Nr_jited_ksyms
	infoLinear.Nr_jited_func_lens = info.Nr_jited_func_lens
	infoLinear.Btf_id = info.Btf_id
	infoLinear.Func_info_rec_size = info.Func_info_rec_size
	infoLinear.Nr_func_info = info.Nr_func_info
	infoLinear.Nr_line_info = info.Nr_line_info
	infoLinear.Nr_jited_line_info = info.Nr_jited_line_info
	infoLinear.Line_info_rec_size = info.Line_info_rec_size
	infoLinear.Jited_line_info_rec_size = info.Jited_line_info_rec_size
	infoLinear.Nr_prog_tags = info.Nr_prog_tags
	infoLinear.Run_time_ns = info.Run_time_ns
	infoLinear.Run_cnt = info.Run_cnt

	return &infoLinear, nil
}
