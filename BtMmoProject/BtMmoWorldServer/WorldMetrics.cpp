#include <Pch.h>
#include "WorldMetrics.h"
#include <NetworkCore/Service.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>

WorldMetrics::WorldMetrics(const std::shared_ptr<Service>& _service)
    : service(_service),
      sessions(prometheus::BuildGauge().Name("btmmo_connected_sessions").Help("Current accepted game sessions.").Register(registry).Add({})),
      sentBytes(prometheus::BuildCounter().Name("btmmo_sent_bytes_total").Help("Bytes completed by game socket sends.").Register(registry).Add({})),
      aiTick(prometheus::BuildHistogram().Name("btmmo_ai_tick_duration_seconds").Help("Elapsed time executing a monster behavior tick.").Register(registry)
                 .Add({}, prometheus::Histogram::BucketBoundaries{0.00005, 0.0001, 0.00025, 0.0005, 0.001, 0.0025, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0}))
{
}

void WorldMetrics::RecordSentBytes(uint32 bytes)
{
	sentBytes.Increment(bytes);
}

void WorldMetrics::ObserveAiTick(std::chrono::steady_clock::duration elapsed)
{
	aiTick.Observe(std::chrono::duration<double>(elapsed).count());
}

std::vector<prometheus::MetricFamily> WorldMetrics::Collect() const
{
	const auto current = service.lock();
	sessions.Set(current ? static_cast<double>(current->GetCurrentSessionCount()) : 0.0);
	return registry.Collect();
}
