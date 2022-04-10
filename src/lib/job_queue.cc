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

#include "job_queue.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

namespace sanescan {

enum JobQueueState {
    STOPPED,
    RUNNING,
    STOPPING
};

struct JobQueue::Private {
    std::queue<IJob*> jobs;
    std::vector<std::thread> threads;
    std::mutex mutex;
    std::condition_variable cv;

    std::atomic<JobQueueState> state = JobQueueState::STOPPED;
};

JobQueue::JobQueue(unsigned thread_count) :
    d_{std::make_unique<Private>()}
{
    d_->threads.resize(thread_count);
}

JobQueue::~JobQueue()
{
    if (d_->state == JobQueueState::RUNNING) {
        stop();
    }
    if (d_->state == JobQueueState::STOPPING) {
        wait();
    }
}

void JobQueue::start()
{
    if (d_->state != JobQueueState::STOPPED) {
        throw std::runtime_error("Can't start running job queue");
    }
    d_->state = JobQueueState::RUNNING;
    for (auto& thread : d_->threads) {
        thread = std::thread([this]()
        {
            while (true) {
                IJob* job = nullptr;
                {
                    std::unique_lock lock{d_->mutex};
                    while (d_->jobs.empty() && d_->state == JobQueueState::RUNNING) {
                        d_->cv.wait(lock);
                    }
                    if (d_->state != JobQueueState::RUNNING) {
                        return;
                    }
                    job = d_->jobs.front();
                    d_->jobs.pop();
                }
                job->execute();
            }
        });
    }
}

void JobQueue::stop()
{
    std::unique_lock lock{d_->mutex};
    if (d_->state != JobQueueState::RUNNING) {
        throw std::runtime_error("Can't stop non-running job queue");
    }
    d_->state = JobQueueState::STOPPING;
    d_->cv.notify_all();
}

void JobQueue::wait()
{
    if (d_->state != JobQueueState::STOPPING) {
        throw std::runtime_error("Can't wait on non-stopping job queue");
    }
    for (auto& thread : d_->threads) {
        thread.join();
    }
    d_->state = JobQueueState::STOPPED;
}

void JobQueue::submit(IJob& job)
{
    std::unique_lock lock{d_->mutex};
    d_->jobs.push(&job);
    d_->cv.notify_one();
}

} // namespace sanescan
