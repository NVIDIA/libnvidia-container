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

package cgroup

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/opencontainers/runtime-spec/specs-go"
)

type DeviceRule = specs.LinuxDeviceCgroup

type Interface interface {
	GetDeviceCGroupMountPath(procRootPath string, pid int) (string, string, error)
	GetDeviceCGroupRootPath(procRootPath string, prefix string, pid int) (string, error)
	AddDeviceRules(cgroupPath string, devices []DeviceRule) error
}

func New(version int) (Interface, error) {
	switch version {
	case 1:
		return &cgroupv1{}, nil
	case 2:
		return &cgroupv2{}, nil
	default:
		return nil, fmt.Errorf("invalid version")
	}
}

type cgroupv1 struct{}
type cgroupv2 struct{}

var _ Interface = (*cgroupv1)(nil)
var _ Interface = (*cgroupv2)(nil)

// GetDeviceCGroupVersion returns the version of linux cgroups in use
func GetDeviceCGroupVersion(rootPath string, pid int) (int, error) {
	// Open the pid's cgroup file in /proc.
	path := fmt.Sprintf(filepath.Join(rootPath, "proc", "%v", "cgroup"), pid)
	file, err := os.Open(path)
	if err != nil {
		return -1, fmt.Errorf("failed to open cgroup path for pid '%d': %v", pid, err)
	}
	defer file.Close()

	// Create a scanner to loop through the file's contents.
	scanner := bufio.NewScanner(file)
	scanner.Split(bufio.ScanLines)

	// Loop through the file looking for either a 'devices' or a '' (i.e. unified) entry
	found := make(map[string]bool)
	for scanner.Scan() {
		parts := strings.SplitN(scanner.Text(), ":", 3)
		if len(parts) != 3 {
			return -1, fmt.Errorf("malformed cgroup entry: %v", scanner.Text())
		}
		found[parts[1]] = true
	}

	// If a 'devices' entry was found, return version 1.
	if found["devices"] {
		return 1, nil
	}

	// If a '', (i.e. 'unified') entry was found, return version 2.
	if found[""] {
		return 2, nil
	}

	return -1, fmt.Errorf("no devices or unified cgroup entries found")
}
