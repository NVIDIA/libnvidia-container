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

package ebpf

import (
	"bytes"
	"encoding/binary"
	"io"
	"unsafe"

	"github.com/cilium/ebpf"
	"github.com/cilium/ebpf/asm"
)

const (
	bpfProgramLicense = "Apache"
)

type Program struct {
	*ebpf.Program
}

var nativeByteOrder binary.ByteOrder

func init() {
	buf := [2]byte{}
	*(*uint16)(unsafe.Pointer(&buf[0])) = uint16(0x00FF)

	switch buf {
	case [2]byte{0xFF, 0x00}:
		nativeByteOrder = binary.LittleEndian
	case [2]byte{0x00, 0xFF}:
		nativeByteOrder = binary.BigEndian
	default:
		panic("Unable to infer byte order")
	}
}

// GetInstructions gets the asm instructions associated with a given ebpf program
func (prog *Program) GetInstructions() (asm.Instructions, string, error) {
	info, err := getProgramInfoLinear(prog)
	if err != nil {
		return nil, "", err
	}
	reader := bytes.NewReader(info.Xlated_prog_insns)

	var insts asm.Instructions
	for {
		var ins asm.Instruction
		_, err := ins.Unmarshal(reader, nativeByteOrder)
		if err == io.EOF {
			return insts, bpfProgramLicense, nil
		}
		if err != nil {
			return nil, "", err
		}
		insts = append(insts, ins)
	}
}
