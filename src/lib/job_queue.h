/*  SPDX-License-Identifier: GPL-3.0-or-later

    Copyright (C) 2021  Povilas Kanapickas <povilas@radix.lt>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef SANESCAN_LIB_JOB_QUEUE_H
#define SANESCAN_LIB_JOB_QUEUE_H

#include <memory>

namespace sanescan {

struct IJob {
    virtual ~IJob() {}
    virtual void execute() = 0;
    virtual void cancel() {}
};

/** This is a naive and very simple job queue. Before implementing improvements alternatives should
    be considered as there are plenty of full-fledged concurrency libraries and it does not make
    sense to reinvent one of them.
*/
class JobQueue {
public:
    JobQueue(unsigned thread_count);
    ~JobQueue();

    /// Starts the worker threads
    void start();

    /// Initiates worker thread shutdown, but does not wait for it.
    void stop();

    /// Waits until worker threads are shut down. The threads are joined with the caller thread.
    void wait();

    /** Submits a job for execution. Does not take ownership of the object. The caller must not
        delete the object until it completes execution.
    */
    void submit(IJob& job);

private:
    struct Private;
    std::unique_ptr<Private> d_;
};

} // namespace sanescan

#endif
