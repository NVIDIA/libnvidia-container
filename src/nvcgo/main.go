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

package main

import (
	"fmt"

	"nvcgo/internal/cgroup"
)

// #include "ctypes.h"
import "C"

// Empty main for c-shared library.
func main() {}

// Alias for a C-based device_rule struct
type CDeviceRule = C.struct_device_rule

// Convert a C-based DeviceRule to a Go-based cgroup.DeviceRule
func (r *CDeviceRule) convert() cgroup.DeviceRule {
	return cgroup.DeviceRule{
		Allow:  bool(r.allow),
		Type:   C.GoString(r._type),
		Access: C.GoString(r.access),
		Major:  func() *int64 { m := int64(r.major); return &m }(),
		Minor:  func() *int64 { m := int64(r.minor); return &m }(),
	}
}

//export GetDeviceCGroupVersion
func GetDeviceCGroupVersion(rootPath *C.char, pid C.pid_t, version *C.int, rerr **C.char) C.int {
	v, err := cgroup.GetDeviceCGroupVersion(C.GoString(rootPath), int(pid))
	if err != nil {
		*rerr = C.CString(err.Error())
		return -1
	}
	*version = C.int(v)
	return 0
}

//export GetDeviceCGroupMountPath
func GetDeviceCGroupMountPath(version C.int, procRootPath *C.char, pid C.pid_t, cgroupMountPath **C.char, cgroupRootPrefix **C.char, rerr **C.char) C.int {
	api, err := cgroup.New(int(version))
	if err != nil {
		*rerr = C.CString(fmt.Sprintf("unable to create cgroupv%v interface: %v", version, err))
		return -1
	}

	p, r, err := api.GetDeviceCGroupMountPath(C.GoString(procRootPath), int(pid))
	if err != nil {
		*rerr = C.CString(err.Error())
		return -1
	}
	*cgroupMountPath = C.CString(p)
	*cgroupRootPrefix= C.CString(r)

	return 0
}

//export GetDeviceCGroupRootPath
func GetDeviceCGroupRootPath(version C.int, procRootPath *C.char, cgroupRootPrefix *C.char, pid C.int, cgroupRootPath **C.char, rerr **C.char) C.int {
	api, err := cgroup.New(int(version))
	if err != nil {
		*rerr = C.CString(fmt.Sprintf("unable to create cgroupv%v interface: %v", version, err))
		return -1
	}

	p, err := api.GetDeviceCGroupRootPath(C.GoString(procRootPath), C.GoString(cgroupRootPrefix), int(pid))
	if err != nil {
		*rerr = C.CString(err.Error())
		return -1
	}
	*cgroupRootPath = C.CString(p)

	return 0
}

//export AddDeviceRules
func AddDeviceRules(version C.int, cgroupPath *C.char, crules []CDeviceRule, rerr **C.char) C.int {
	api, err := cgroup.New(int(version))
	if err != nil {
		*rerr = C.CString(fmt.Sprintf("unable to create cgroupv%v interface: %v", version, err))
		return -1
	}

	rules := make([]cgroup.DeviceRule, len(crules))
	for i, cr := range crules {
		rules[i] = cr.convert()
	}

	err = api.AddDeviceRules(C.GoString(cgroupPath), rules)
	if err != nil {
		*rerr = C.CString(err.Error())
		return -1
	}

	return 0
}
