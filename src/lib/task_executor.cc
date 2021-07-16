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

#include "task_executor.h"
#include <deque>

namespace sanescan {

struct TaskExecutor::Impl {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::unique_ptr<ITask>> tasks;
    std::thread thread;
    std::atomic_bool active = false;
    std::atomic_bool stop = false;
};

TaskExecutor::TaskExecutor() :
    impl_{std::make_unique<Impl>()}
{
    impl_->thread = std::thread([this]()
    {
        std::unique_ptr<ITask> task;

        while (true) {
            {
                std::unique_lock lock{impl_->mutex};

                impl_->active = false;
                impl_->cv.wait(lock, [this](){ return !impl_->tasks.empty() || impl_->stop; });

                if (impl_->tasks.empty()) {
                    // if task list is empty at this point, stop has been requested (see
                    // the condition-variable condition above)
                    break;
                }
                impl_->active = true;

                task = std::move(impl_->tasks.front());
                impl_->tasks.pop_front();
            }
            task->call();
            task.reset();
        }
    });
}

TaskExecutor::~TaskExecutor()
{
    if (impl_->thread.joinable())
        join();
}

void TaskExecutor::join()
{
    {
        std::unique_lock lock{impl_->mutex};
        impl_->stop = true;
        impl_->cv.notify_all();
    }
    impl_->thread.join();
}

void TaskExecutor::join_cancel()
{
    {
        std::unique_lock lock{impl_->mutex};
        impl_->stop = true;
        impl_->tasks.clear();
        impl_->cv.notify_all();
    }
    impl_->thread.join();
}

void TaskExecutor::schedule_task_impl(std::unique_ptr<ITask>&& task)
{
    std::unique_lock lock{impl_->mutex};
    if (!impl_->thread.joinable()) {
        throw std::runtime_error("Execution thread has already been stopped");
    }

    impl_->tasks.push_back(std::move(task));
    impl_->cv.notify_all();
}

bool TaskExecutor::active() const
{
    std::unique_lock lock{impl_->mutex};
    return !impl_->tasks.empty() || impl_->active;
}

} // namespace sanescan
