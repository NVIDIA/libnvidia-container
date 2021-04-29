/*
# Copyright (c) 2021, NVIDIA CORPORATION.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

podTemplate (cloud:'sw-gpu-cloudnative',
    containers: [
    containerTemplate(name: 'docker', image: 'docker:dind', ttyEnabled: true, privileged: true)
  ]) {
    node(POD_LABEL) {
        stage('checkout') {
            checkout scm
        }
        stage('dependencies') {
            container('docker') {
                sh 'apk add --no-cache make bash'
            }
        }
        stage('build-one') {
            container('docker') {
                def dist = 'ubuntu18.04'
                def arch = 'arm64'
                stage("${dist}-${arch}") {
                    sh "make -f mk/docker.mk ${dist}-${arch}"
                }
            }
        }
    }
}