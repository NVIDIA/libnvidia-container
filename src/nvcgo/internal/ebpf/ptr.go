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
// https://github.com/cilium/ebpf/blob/b38550c6f15200e3798c695890f699001b97e229/internal/ptr.go

package ebpf

import (
	"unsafe"
)

// NewPointer creates a 64-bit pointer from an unsafe Pointer.
func NewPointer(ptr unsafe.Pointer) Pointer {
	return Pointer{ptr: ptr}
}

// NewSlicePointer creates a 64-bit pointer from a byte slice.
func NewSlicePointer(buf []byte) Pointer {
	if len(buf) == 0 {
		return Pointer{}
	}

	return Pointer{ptr: unsafe.Pointer(&buf[0])}
}
