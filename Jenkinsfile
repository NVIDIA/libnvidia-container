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
        def scmInfo

        stage('checkout') {
            scmInfo = checkout(scm)
        }

        stage('dependencies') {
            container('docker') {
                sh 'apk add --no-cache make bash git'
            }
        }

        def versionInfo
        stage('version') {
            container('docker') {
                versionInfo = getVersionInfo(scmInfo)
                println "versionInfo=${versionInfo}"
            }
        }

        def dist = 'ubuntu18.04'
        def arch = 'arm64'
        def stageLabel = "${dist}-${arch}"

        stage('build-one') {
            container('docker') {
                stage (stageLabel) {
                    sh "make -f mk/docker.mk ${dist}-${arch}"
                }
            }
        }

        stage('release') {
            container('docker') {
                stage (stageLabel) {

                    def component = 'jetpack'
                    def repository = 'sw-gpu-cloudnative-debian-local/pool/jetpack'

                    def uploadSpec = """{
                                        "files":
                                        [  {
                                                "pattern": "./dist/${dist}/${arch}/*.deb",
                                                "target": "${repository}",
                                                "props": "deb.distribution=${dist};deb.component=${component};deb.architecture=${arch}"
                                            }
                                        ]
                                    }"""

                    sh "echo starting release with versionInfo=${versionInfo}"
                    if (versionInfo.isTag) {
                        // upload to artifactory repository
                        def server = Artifactory.server 'sw-gpu-artifactory'
                        server.upload spec: uploadSpec
                    } else {
                        sh "echo skipping release for non-tagged build"
                    }
                }
            }
        }
    }
}

// getVersionInfo returns a hash of version info
def getVersionInfo(def scmInfo) {
    def version = shOuptut('git describe --tags')
    def isTag = false

    def isMaster = scmInfo.GIT_BRANCH == "origin/master"
    def isJetson = scmInfo.GIT_BRANCH == "origin/jetson"

    if (version == getLastTag()) {
        isTag = true
        isMaster = false
        isJetson = false
    }

    def versionInfo = [
        version: sanitizeVersion(version),
        isMaster: isMaster,
        isJetson: isJetson
        isTag: isTag
    ]

    scmInfo.each { k, v -> versionInfo[k] = v }
    return versionInfo
}

// getLastTag returns the last tag in the repo
def getLastTag() {
    return shOuptut('git describe --tags $(git rev-list --tags --max-count=1)')
}

// sanitizeVersion ensures that the version can be used to build the libnvidiaContainer
// package
def sanitizeVersion(def version) {
    if (version == '') {
        return version
    }

    version = version.startsWith('v') ? version - 'v' : version

    def sanitized = ''
    def sep = ''
    version.split('-').eachWithIndex { it, i ->
        sanitized = "${sanitized}${sep}${it}"
        if (i == 0) {
            sep = '-'
        } else {
            sep = '~'
        }
    }

    return sanitized
}

def shOuptut(def script) {
    return sh(script: script, returnStdout: true).trim()
}
