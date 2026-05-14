#pragma once

class IJob
{
public:
	virtual ~IJob() = default;
	virtual void Run() = 0;
};

class JobQueue
{
public:
	static JobQueue* GetSingleton()
	{
		static JobQueue singleton;
		return &singleton;
	}

	void Enqueue(std::unique_ptr<IJob> a_job);
	void ProcessAll();
	bool HasPending() const;

private:
	JobQueue() = default;
	mutable std::mutex mutex;
	std::queue<std::unique_ptr<IJob>> jobs;
};

class SaveConfigJob : public IJob
{
public:
	SaveConfigJob(std::filesystem::path a_path, nlohmann::json a_json)
		: path(std::move(a_path)), json(std::move(a_json)) {}
	void Run() override;
private:
	std::filesystem::path path;
	nlohmann::json json;
};

class ReloadConfigJob : public IJob
{
public:
	void Run() override;
};

class LambdaJob : public IJob
{
public:
	LambdaJob(std::function<void()> a_fn) : fn(std::move(a_fn)) {}
	void Run() override { fn(); }
private:
	std::function<void()> fn;
};
