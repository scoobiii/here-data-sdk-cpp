/*
 * Copyright (C) 2020 HERE Europe B.V.
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
 *
 * SPDX-License-Identifier: Apache-2.0
 * License-Filename: LICENSE
 */

#include "TaskSink.h"

#include <olp/core/logging/Log.h>

namespace olp {
namespace dataservice {
namespace read {
namespace {
constexpr auto kLogTag = "TaskSink";
}

TaskSink::TaskSink(std::shared_ptr<thread::TaskScheduler> task_scheduler)
    : task_scheduler_(std::move(task_scheduler)),
      pending_requests_(std::make_shared<client::PendingRequests>()),
      closed_(false) {}

TaskSink::~TaskSink() {
  OLP_SDK_LOG_INFO(kLogTag, "Finishing, canceling all current tasks.");
  {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
  }
  // CancelAllAndWait method should be called without mutex, since potentially
  // there might be new added tasks, it may result in deadlock.
  pending_requests_->CancelAllAndWait();
}

void TaskSink::CancelTasks() { pending_requests_->CancelAll(); }

bool TaskSink::AddTaskImpl(client::TaskContext task, uint32_t priority) {
  if (task_scheduler_) {
    return ScheduleTask(std::move(task), priority);
  } else {
    ExecuteTask(std::move(task));
    return true;
  }
}

bool TaskSink::ScheduleTask(client::TaskContext task, uint32_t priority) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (closed_) {
    OLP_SDK_LOG_WARNING(
        kLogTag, "Attempt to add a task when the sink is already closed");
    return false;
  }

  pending_requests_->Insert(task);
  auto pending_requests = pending_requests_;
  task_scheduler_->ScheduleTask(
      [=] {
        task.Execute();
        pending_requests->Remove(task);
      },
      priority);

  return true;
}

void TaskSink::ExecuteTask(client::TaskContext task) { task.Execute(); }

}  // namespace read
}  // namespace dataservice
}  // namespace olp
