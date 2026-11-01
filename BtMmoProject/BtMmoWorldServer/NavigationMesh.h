#pragma once

struct NavPoint
{
	float x{}, y{}, z{};
};

class NavigationMesh
{
public:
	NavigationMesh();
	~NavigationMesh();
	// Load/destruction must not overlap queries. Completed loads are shared read-only;
	// synchronous queries borrow independent scratch storage on each calling thread.
	std::expected<void, std::string> Load(const std::filesystem::path& path);
	std::expected<std::vector<NavPoint>, std::string> FindPath(NavPoint start, NavPoint end) const;
	std::expected<NavPoint, std::string> ProjectPoint(NavPoint point) const;
	std::expected<NavPoint, std::string> TraceSurface(NavPoint start, NavPoint end) const;
	std::expected<NavPoint, std::string> RandomPoint(std::mt19937& random) const;
	std::expected<NavPoint, std::string> RandomReachablePoint(NavPoint center, float radius, std::mt19937& random) const;
	const std::string& MapName() const;
	unsigned TileCount() const;
	unsigned PolygonCount() const;
private:
	struct State;
	std::unique_ptr<State> state;
};
