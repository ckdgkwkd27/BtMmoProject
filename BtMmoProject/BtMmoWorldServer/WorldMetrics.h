#pragma once
#include <Types.h>
#include <prometheus/registry.h>

class Service;

class WorldMetrics final : public prometheus::Collectable
{
public:
	explicit WorldMetrics(const std::shared_ptr<Service>& service);
	void RecordSentBytes(uint32 bytes);
	void ObserveAiTick(std::chrono::steady_clock::duration elapsed);
	std::vector<prometheus::MetricFamily> Collect() const override;
private:
	std::weak_ptr<Service> service;
	prometheus::Registry registry;
	prometheus::Gauge& sessions;
	prometheus::Counter& sentBytes;
	prometheus::Histogram& aiTick;
};
