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

#include <sys/sysmacros.h>

#include "cgroup.h"
#include "error.h"
#include "options.h"

typedef char *(*parse_fn)(char *, char *, const char *);

static char *cgroup_mount(char *, char *, const char *);
static char *cgroup_root(char *, char *, const char *);
static char *parse_proc_file(struct error *, const char *, parse_fn, char *, const char *);

char *
find_device_cgroup_path(struct error *err, const struct nvc_container *cnt)
{
        pid_t pid;
        const char *prefix;
        char path[PATH_MAX];
        char root_prefix[PATH_MAX];
        char *mount = NULL;
        char *root = NULL;
        char *cgroup = NULL;

        pid = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.pid : getppid();
        prefix = (cnt->flags & OPT_STANDALONE) ? cnt->cfg.rootfs : "";

        if (xsnprintf(err, path, sizeof(path), "%s"PROC_MOUNTS_PATH(PROC_PID), prefix, (int32_t)pid) < 0)
                goto fail;
        if ((mount = parse_proc_file(err, path, cgroup_mount, root_prefix, "devices")) == NULL)
                goto fail;
        if (xsnprintf(err, path, sizeof(path), "%s"PROC_CGROUP_PATH(PROC_PID), prefix, (int32_t)cnt->cfg.pid) < 0)
                goto fail;
        if ((root = parse_proc_file(err, path, cgroup_root, root_prefix, "devices")) == NULL)
                goto fail;

        xasprintf(err, &cgroup, "%s%s%s", prefix, mount, root);

 fail:
        free(mount);
        free(root);
        return (cgroup);
}

int
setup_device_cgroup(struct error *err, const struct nvc_container *cnt, dev_t id)
{
        char path[PATH_MAX];
        FILE *fs;
        int rv = -1;

        if (path_join(err, path, cnt->dev_cg, "devices.allow") < 0)
                return (-1);
        if ((fs = xfopen(err, path, "a")) == NULL)
                return (-1);

        log_infof("whitelisting device node %u:%u", major(id), minor(id));
        /* XXX dprintf doesn't seem to catch the write errors, flush the stream explicitly instead. */
        if (fprintf(fs, "c %u:%u rw", major(id), minor(id)) < 0 || fflush(fs) == EOF || ferror(fs)) {
                error_set(err, "write error: %s", path);
                goto fail;
        }
        rv = 0;

 fail:
        fclose(fs);
        return (rv);
}

static char *
cgroup_mount(char *line, char *prefix, const char *subsys)
{
        char *root, *mount, *fstype, *substr;

        for (int i = 0; i < 4; ++i)
                root = strsep(&line, " ");
        mount = strsep(&line, " ");
        line = strchr(line, '-');
        for (int i = 0; i < 2; ++i)
                fstype = strsep(&line, " ");
        for (int i = 0; i < 2; ++i)
                substr = strsep(&line, " ");

        if (root == NULL || mount == NULL || fstype == NULL || substr == NULL)
                return (NULL);
        if (*root == '\0' || *mount == '\0' || *fstype == '\0' || *substr == '\0')
                return (NULL);
        if (!str_equal(fstype, "cgroup"))
                return (NULL);
        if (strstr(substr, subsys) == NULL)
                return (NULL);
        if (strlen(root) >= PATH_MAX || str_has_prefix(root, "/.."))
                return (NULL);
        strcpy(prefix, root);

        return (mount);
}

static char *
cgroup_root(char *line, char *prefix, const char *subsys)
{
        char *heirarchy_id, *controller_list, *cgroup_path;

        // From: https://man7.org/linux/man-pages/man7/cgroups.7.html
        // The lines of the /proc/{pid}/cgroup file have the following format:
        //     hierarchy-ID:controller-list:cgroup-path
        // Here we attempt to parse the separate sections. If this is not
        // possible, we return NULL
        heirarchy_id = strsep(&line, ":");
        if (heirarchy_id == NULL) {
                // line contained no colons
                return (NULL);
        }
        controller_list = strsep(&line, ":");
        if (controller_list == NULL) {
                // line contains only a single colon
                return (NULL);
        }
        // Since strsep modifies the pointer *line,
        // the remaining string is the cgroup path
        cgroup_path = line;
        if (cgroup_path == NULL) {
                return (NULL);
        }
        if (*cgroup_path == '\0' || *controller_list == '\0') {
                // The controller list or cgroup_path are empty strings
                return (NULL);
        }
        if (strstr(controller_list, subsys) == NULL) {
                // The desired subsystem name is not in the controller list
                return (NULL);
        }
        if (strlen(cgroup_path) >= PATH_MAX || str_has_prefix(cgroup_path, "/..")) {
                // The cgroup path is malformed: It is too long or is a relative path
                return (NULL);
        }
        if (!str_equal(prefix, "/") && str_has_prefix(cgroup_path, prefix)) {
                // Strip the supplied prefix from the cgroup path unless
                // it is a "/"
                cgroup_path += strlen(prefix);
        }

        return (cgroup_path);
}

static char *
parse_proc_file(struct error *err, const char *procf, parse_fn parse, char *prefix, const char *subsys)
{
        FILE *fs;
        ssize_t n;
        char *buf = NULL;
        size_t len = 0;
        char *ptr = NULL;
        char *path = NULL;

        if ((fs = xfopen(err, procf, "r")) == NULL)
                return (NULL);
        while ((n = getline(&buf, &len, fs)) >= 0) {
                ptr = buf;
                ptr[strcspn(ptr, "\n")] = '\0';
                if (n == 0 || *ptr == '\0')
                        continue;
                if ((ptr = parse(ptr, prefix, subsys)) != NULL)
                        break;
        }
        if (ferror(fs)) {
                error_set(err, "read error: %s", procf);
                goto fail;
        }
        if (ptr == NULL || feof(fs)) {
                error_setx(err, "cgroup subsystem %s not found", subsys);
                goto fail;
        }
        path = xstrdup(err, ptr);

 fail:
        free(buf);
        fclose(fs);
        return (path);
}
