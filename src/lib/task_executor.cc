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

struct TaskExecutor::Private {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::unique_ptr<ITask>> tasks;
    std::thread thread;
    std::atomic_bool active = false;
    std::atomic_bool stop = false;
};

TaskExecutor::TaskExecutor() :
    d_{std::make_unique<Private>()}
{
    d_->thread = std::thread([this]()
    {
        std::unique_ptr<ITask> task;

        while (true) {
            {
                std::unique_lock lock{d_->mutex};

                d_->active = false;
                d_->cv.wait(lock, [this](){ return !d_->tasks.empty() || d_->stop; });

                if (d_->tasks.empty()) {
                    // if task list is empty at this point, stop has been requested (see
                    // the condition-variable condition above)
                    break;
                }
                d_->active = true;

                task = std::move(d_->tasks.front());
                d_->tasks.pop_front();
            }
            task->call();
            task.reset();
        }
    });
}

TaskExecutor::~TaskExecutor()
{
    if (d_->thread.joinable())
        join();
}

void TaskExecutor::join()
{
    {
        std::unique_lock lock{d_->mutex};
        d_->stop = true;
        d_->cv.notify_all();
    }
    d_->thread.join();
}

void TaskExecutor::join_cancel()
{
    {
        std::unique_lock lock{d_->mutex};
        d_->stop = true;
        d_->tasks.clear();
        d_->cv.notify_all();
    }
    d_->thread.join();
}

void TaskExecutor::schedule_task_impl(std::unique_ptr<ITask>&& task)
{
    std::unique_lock lock{d_->mutex};
    if (!d_->thread.joinable()) {
        throw std::runtime_error("Execution thread has already been stopped");
    }

    d_->tasks.push_back(std::move(task));
    d_->cv.notify_all();
}

bool TaskExecutor::active() const
{
    std::unique_lock lock{d_->mutex};
    return !d_->tasks.empty() || d_->active;
}

} // namespace sanescan
