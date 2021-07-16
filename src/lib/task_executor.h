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

#ifndef SANESCAN_LIB_TASK_EXECUTOR_H
#define SANESCAN_LIB_TASK_EXECUTOR_H

#include <future>
#include <memory>

namespace sanescan {

/** A simple task executor that executes tasks serially in a single thread.

    Performance was not taken into account when developing this code, so if it ever becomes a
    bottleneck significant performance optimizations are possible.
*/
class TaskExecutor {
public:
    TaskExecutor();
    ~TaskExecutor();

    template<class R, class F>
    std::future<R> schedule_task(F&& callable)
    {
        auto task = std::make_unique<Task<R>>(callable);
        auto future = task->get_future();
        schedule_task_impl(std::move(task));
        return future;
    }

    // returns true if there are pending tasks or the underlying thread is processing one.
    bool active() const;

    // waits until all tasks are done and stops the execution thread
    void join();

    // same as join() except all pending tasks in the queue are cancelled.
    void join_cancel();

private:
    struct ITask {
        virtual void call() = 0;
        virtual ~ITask() = default;
    };

    template<class R>
    struct Task : ITask {
        template<class F>
        Task(F&& callable) : task_{std::forward<F>(callable)} {}

        std::future<R> get_future() { return task_.get_future(); }

        void call() override
        {
            task_();
        }

    private:
        std::packaged_task<R()> task_;
    };

    void schedule_task_impl(std::unique_ptr<ITask>&& task);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sanescan

#endif
