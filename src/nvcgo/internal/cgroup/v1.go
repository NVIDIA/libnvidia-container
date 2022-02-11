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
)

// GetDeviceCGroupMountPath returns the mount path (and its prefix) for the device cgroup controller associated with pid
func (c *cgroupv1) GetDeviceCGroupMountPath(procRootPath string, pid int) (string, string, error) {
	// Open the pid's mountinfo file in /proc.
	path := fmt.Sprintf(filepath.Join(procRootPath, "proc", "%v", "mountinfo"), pid)
	file, err := os.Open(path)
	if err != nil {
		return "", "", err
	}
	defer file.Close()

	// Create a scanner to loop through the file's contents.
	scanner := bufio.NewScanner(file)
	scanner.Split(bufio.ScanLines)

	// Loop through the file looking for a subsystem of 'devices' entry.
	for scanner.Scan() {
		// Split each entry by '[space]'
		parts := strings.Split(scanner.Text(), " ")
		if len(parts) < 5 {
			return "", "", fmt.Errorf("malformed mountinfo entry: %v", scanner.Text())
		}
		// Look for an entry with cgroup as the mount type.
		if parts[len(parts)-3] != "cgroup" {
			continue
		}
		// Look for an entry with 'devices' as the basename of the mountpath
		if filepath.Base(parts[4]) != "devices" {
			continue
		}
		// Make sure the mount prefix is not a relative path.
		if strings.HasPrefix(parts[3], "/..") {
			return "", "", fmt.Errorf("relative path in mount prefix: %v", parts[3])
		}
		// Return the 3rd element as the prefix of the mount point for
		// the devices cgroup and the 4th element as the mount point of
		// the devices cgroup itself.
		return parts[3], parts[4], nil
	}

	return "", "", fmt.Errorf("no cgroup filesystem mounted for the devices subsytem in mountinfo file")
}

// GetDeviceCGroupRootPath returns the root path for the device cgroup controller associated with pid
func (c *cgroupv1) GetDeviceCGroupRootPath(procRootPath string, prefix string, pid int) (string, error) {
	// Open the pid's cgroup file in /proc.
	path := fmt.Sprintf(filepath.Join(procRootPath, "proc", "%v", "cgroup"), pid)
	file, err := os.Open(path)
	if err != nil {
		return "", err
	}
	defer file.Close()

	// Create a scanner to loop through the file's contents.
	scanner := bufio.NewScanner(file)
	scanner.Split(bufio.ScanLines)

	// Loop through the file looking for either a subsystem of 'devices' entry.
	for scanner.Scan() {
		// Split each entry by ':'
		parts := strings.SplitN(scanner.Text(), ":", 3)
		if len(parts) != 3 {
			return "", fmt.Errorf("malformed cgroup entry: %v", scanner.Text())
		}
		// Look for the devices subsystem in the 1st element.
		if parts[1] != "devices" {
			continue
		}
		// Return the cgroup root from the 2nd element
		// (with the prefix possibly stripped off).
		if prefix == "/" {
			return parts[2], nil
		}
		return strings.TrimPrefix(parts[2], prefix), nil
	}

	return "", fmt.Errorf("no devices cgroup entries found")
}

// AddDeviceRules adds a set of device rules for the device cgroup at cgroupPath
func (c *cgroupv1) AddDeviceRules(cgroupPath string, rules []DeviceRule) error {
	// Loop through all rules in the set of device rules and add that rule to the device.
	for _, rule := range rules {
		err := c.addDeviceRule(cgroupPath, &rule)
		if err != nil {
			return err
		}
	}

	return nil
}

func (c *cgroupv1) addDeviceRule(cgroupPath string, rule *DeviceRule) error {
	// Check the major/minor numbers of the device in the device rule.
	if rule.Major == nil {
		return fmt.Errorf("no major set in device rule")
	}

	if rule.Minor == nil {
		return fmt.Errorf("no minor set in device rule")
	}

	// Open the appropriate allow/deny file.
	var path string
	if rule.Allow {
		path = filepath.Join(cgroupPath, "devices.allow")
	} else {
		path = filepath.Join(cgroupPath, "devices.deny")
	}
	file, err := os.OpenFile(path, os.O_APPEND|os.O_WRONLY, 0600)
	if err != nil {
		return err
	}
	defer file.Close()

	// Write the device rule into the file.
	_, err = file.WriteString(fmt.Sprintf("%s %d:%d %s", rule.Type, *rule.Major, *rule.Minor, rule.Access))
	if err != nil {
		return err
	}

	return nil
}
