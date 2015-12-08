/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LIBBACKTRACE_THREAD_UTILS_H
#define _LIBBACKTRACE_THREAD_UTILS_H

#include <unistd.h>

#if !defined(__ANDROID__)
#include <cutils/threads.h>
#endif

__BEGIN_DECLS

int tgkill(int tgid, int tid, int sig);

__END_DECLS

#endif /* _LIBBACKTRACE_THREAD_UTILS_H */
