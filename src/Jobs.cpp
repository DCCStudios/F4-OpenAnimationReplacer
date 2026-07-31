#include "Jobs.h"
#include "Hooks.h"
#include "Parsing.h"
#include "OpenAnimationReplacer.h"

void JobQueue::Enqueue(std::unique_ptr<IJob> a_job)
{
	std::lock_guard lock(mutex);
	jobs.push(std::move(a_job));
}

void JobQueue::ProcessAll()
{
	std::queue<std::unique_ptr<IJob>> localQueue;
	{
		std::lock_guard lock(mutex);
		std::swap(localQueue, jobs);
	}

	while (!localQueue.empty()) {
		auto& job = localQueue.front();
		try {
			job->Run();
		} catch (const std::exception& e) {
			logger::error("[OAR] Job execution failed: {}", e.what());
		}
		localQueue.pop();
	}
}

bool JobQueue::HasPending() const
{
	std::lock_guard lock(mutex);
	return !jobs.empty();
}

void SaveConfigJob::Run()
{
	Parsing::SaveJsonFile(path, json);
}

void ReloadConfigJob::Run()
{
	// The job queue drains on the UI (render) thread, but the reload frees
	// objects that live graph updates hold pointers to — it must run on the
	// game thread. Defer to Hooks' game-thread drain (HookedActorUpdate).
	RequestConfigReload();
}
