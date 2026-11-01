#include <Pch.h>
#include "NavigationMesh.h"
#include <boost/json.hpp>
#include <DetourAlloc.h>
#include <DetourNavMesh.h>
#include <DetourNavMeshBuilder.h>
#include <DetourNavMeshQuery.h>

struct MeshDeleter
{
	void operator()(dtNavMesh* p) const
	{
		dtFreeNavMesh(p);
	}
};

struct QueryDeleter
{
	void operator()(dtNavMeshQuery* p) const
	{
		dtFreeNavMeshQuery(p);
	}
};

class NavigationQuery final
{
public:
	static NavigationQuery Borrow(const dtNavMesh* mesh)
	{
		static thread_local Storage storage;
		return NavigationQuery(storage, mesh);
	}

	NavigationQuery(const NavigationQuery&) = delete;
	NavigationQuery& operator=(const NavigationQuery&) = delete;
	~NavigationQuery() { Return(); }

	explicit operator bool() const { return storage != nullptr; }

	dtNavMeshQuery* operator->() const { return storage->query.get(); }

	dtNavMeshQuery& operator*() const { return *storage->query; }
private:
	static constexpr int MAX_NODES = 8192;
	struct Storage
	{
		std::unique_ptr<dtNavMeshQuery, QueryDeleter> query;
		bool borrowed{};
	};

	NavigationQuery(Storage& cache, const dtNavMesh* mesh)
	{
		if (cache.borrowed)
		{
			return;
		}

		// Reserve before allocation so a reentrant call cannot reuse this query.
		cache.borrowed = true;
		if (!cache.query)
		{
			cache.query.reset(dtAllocNavMeshQuery());
		}

		if (!cache.query || dtStatusFailed(cache.query->init(mesh, MAX_NODES)))
		{
			cache.query.reset();
			cache.borrowed = false;
			return;
		}

		storage = &cache;
	}

	void Return() noexcept
	{
		if (!storage)
		{
			return;
		}

		storage->query->init(nullptr, MAX_NODES);
		storage->borrowed = false;
		storage = nullptr;
	}

	Storage* storage{};
};

static bool Number(const boost::json::value& value, double& number)
{
	if (value.is_double())
	{
		number = value.as_double();
	}
	else if (value.is_int64())
	{
		number = static_cast<double>(value.as_int64());
	}
	else if (value.is_uint64())
	{
		number = static_cast<double>(value.as_uint64());
	}
	else
	{
		return false;
	}

	return std::isfinite(number);
}

template <typename T>
static bool Scalar(const boost::json::value& value, T& output)
{
	double number{};
	if (!Number(value, number) || number < static_cast<double>(std::numeric_limits<T>::lowest()) || number > static_cast<double>(std::numeric_limits<T>::max()))
	{
		return false;
	}

	if constexpr (std::is_integral_v<T>)
	{
		if (std::trunc(number) != number)
		{
			return false;
		}
	}

	output = static_cast<T>(number);
	return true;
}

template <typename T>
static bool Field(const boost::json::object& object, const char* key, T& output)
{
	const auto* value = object.if_contains(key);
	return value && Scalar(*value, output);
}

static bool Text(const boost::json::object& object, const char* key, std::string& output)
{
	const auto* value = object.if_contains(key);
	if (!value || !value->is_string())
	{
		return false;
	}

	const auto& string = value->as_string();
	output.assign(string.data(), string.size());
	return true;
}

template <typename T>
static bool Array(const boost::json::object& object, const char* key, std::vector<T>& output, std::size_t maximum)
{
	const auto* value = object.if_contains(key);
	if (!value || !value->is_array() || value->as_array().size() > maximum)
	{
		return false;
	}

	output.resize(value->as_array().size());
	for (std::size_t i = 0; i < output.size(); ++i)
	{
		if (!Scalar(value->as_array()[i], output[i]))
		{
			return false;
		}
	}

	return true;
}

static bool Vector(const boost::json::object& object, const char* key, float* output)
{
	std::vector<float> values;
	if (!Array(object, key, values, 3) || values.size() != 3)
	{
		return false;
	}

	std::ranges::copy(values, output);
	return true;
}

struct Tile
{
	dtNavMeshCreateParams params{};
	std::vector<unsigned short> vertices, polygons, flags;
	std::vector<unsigned char> areas, triangles;
	std::vector<unsigned int> details;
	std::vector<float> detailVertices;
};

static std::expected<Tile, std::string> ReadTile(const boost::json::object& object)
{
	Tile tile;
	auto& p = tile.params;
	if (!Field(object, "x", p.tileX) || !Field(object, "y", p.tileY) || !Field(object, "layer", p.tileLayer) || p.tileLayer < 0 ||
	    !Field(object, "cellSize", p.cs) || !Field(object, "cellHeight", p.ch) || p.cs <= 0 || p.ch <= 0 ||
	    !Field(object, "walkableClimb", p.walkableClimb) || p.walkableClimb < 0 ||
	    !Vector(object, "bmin", p.bmin) || !Vector(object, "bmax", p.bmax))
	{
		return std::unexpected("Invalid tile metadata");
	}

	for (int i = 0; i < 3; ++i)
	{
		if (p.bmax[i] < p.bmin[i])
		{
			return std::unexpected("Invalid tile bounds");
		}
	}

	if (!Array(object, "vertices", tile.vertices, 65534 * 3) || tile.vertices.empty() || tile.vertices.size() % 3 ||
	    !Array(object, "polygons", tile.polygons, 32767 * 12) || tile.polygons.empty() || tile.polygons.size() % 12 ||
	    !Array(object, "flags", tile.flags, 32767) || !Array(object, "areas", tile.areas, 32767) ||
	    !Array(object, "detailMeshes", tile.details, 32767 * 4) ||
	    !Array(object, "detailVertices", tile.detailVertices, 1048576) || tile.detailVertices.size() % 3 ||
	    !Array(object, "detailTriangles", tile.triangles, 65535 * 4) || tile.triangles.size() % 4)
	{
		return std::unexpected("Invalid tile arrays");
	}

	p.vertCount = static_cast<int>(tile.vertices.size() / 3);
	p.polyCount = static_cast<int>(tile.polygons.size() / 12);
	p.detailVertsCount = static_cast<int>(tile.detailVertices.size() / 3);
	p.detailTriCount = static_cast<int>(tile.triangles.size() / 4);
	if (tile.flags.size() != p.polyCount || tile.areas.size() != p.polyCount || tile.details.size() != p.polyCount * 4)
	{
		return std::unexpected("Tile array counts disagree");
	}

	unsigned extraVertices = 0;
	for (int i = 0; i < p.polyCount; ++i)
	{
		unsigned count = 0;
		bool ended = false;
		for (int edge = 0; edge < 6; ++edge)
		{
			const auto vertex = tile.polygons[i * 12 + edge];
			const auto neighbor = tile.polygons[i * 12 + 6 + edge];
			if (vertex == 65535)
			{
				ended = true;
				continue;
			}

			if (ended || vertex >= p.vertCount)
			{
				return std::unexpected("Invalid polygon vertex index");
			}

			for (int earlier = 0; earlier < edge; ++earlier)
			{
				if (tile.polygons[i * 12 + earlier] == vertex)
				{
					return std::unexpected("Repeated polygon vertex");
				}
			}

			++count;
			if (neighbor != 65535 && neighbor != 0x8000 && neighbor != 0x8001 && neighbor != 0x8002 && neighbor != 0x8003 && neighbor >= p.polyCount)
			{
				return std::unexpected("Invalid polygon neighbor index");
			}
		}

		const auto* d = &tile.details[i * 4];
		if (count < 3 || tile.areas[i] >= DT_MAX_AREAS || d[1] < count || d[1] > 255 || d[3] == 0 || d[3] > 255 ||
		    d[0] > static_cast<unsigned>(p.detailVertsCount) || d[1] > static_cast<unsigned>(p.detailVertsCount) - d[0] ||
		    d[2] > static_cast<unsigned>(p.detailTriCount) || d[3] > static_cast<unsigned>(p.detailTriCount) - d[2])
		{
			return std::unexpected("Invalid detail mesh range");
		}

		extraVertices += d[1] - count;
		for (unsigned t = d[2]; t < d[2] + d[3]; ++t)
		{
			for (unsigned k = 0; k < 3; ++k)
			{
				if (tile.triangles[t * 4 + k] >= d[1])
				{
					return std::unexpected("Invalid detail triangle vertex index");
				}
			}
		}
	}

	if (extraVertices > 65535)
	{
		return std::unexpected("Too many detail vertices for Detour 1.6");
	}

	return tile;
}

static std::array<float, 3> ToRecast(NavPoint p)
{
	return {-p.x, p.z, -p.y};
}

static NavPoint ToUnreal(const float* p)
{
	return {.x = -p[0], .y = -p[2], .z = p[1]};
}

static thread_local std::mt19937* queryRandom = nullptr;
struct RandomScope
{
	std::mt19937* previous;
	explicit RandomScope(std::mt19937& random) : previous(queryRandom) { queryRandom = &random; }

	~RandomScope() { queryRandom = previous; }
};

static float RandomFraction()
{
	return static_cast<float>((*queryRandom)() >> 8) * (1.0f / 16777216.0f);
}

constexpr float PROJECTION_EXTENTS[] = {80, 120, 80};
static bool Project(dtNavMeshQuery& query, const dtQueryFilter& filter, NavPoint point, dtPolyRef& ref, float* projected)
{
	const auto p = ToRecast(point);
	if (!std::ranges::all_of(p, [](float v)
	{
		return std::isfinite(v);
	}))
	{
		return false;
	}

	std::array<dtPolyRef, 256> candidates{};
	int count{};
	const auto status = query.queryPolygons(p.data(), PROJECTION_EXTENTS, &filter, candidates.data(), &count, static_cast<int>(candidates.size()));
	if (dtStatusFailed(status) || dtStatusDetail(status, DT_BUFFER_TOO_SMALL))
	{
		return false;
	}

	double best = std::numeric_limits<double>::max();
	ref = 0;
	for (int index = 0; index < count; ++index)
	{
		float closest[3]{};
		if (dtStatusFailed(query.closestPointOnPoly(candidates[index], p.data(), closest, nullptr)))
		{
			continue;
		}

		double distance = 0;
		bool within = true;
		for (int axis = 0; axis < 3; ++axis)
		{
			const double delta = closest[axis] - p[axis];
			if (!std::isfinite(delta) || std::abs(delta) > PROJECTION_EXTENTS[axis])
			{
				within = false;
			}

			distance += delta * delta;
		}

		if (within && distance < best)
		{
			best = distance;
			ref = candidates[index];
			std::copy_n(closest, 3, projected);
		}
	}

	return ref != 0;
}

struct NavigationMesh::State
{
	std::unique_ptr<dtNavMesh, MeshDeleter> mesh;
	dtQueryFilter filter;
	std::string map;
	unsigned tiles{}, polygons{};
};

NavigationMesh::NavigationMesh() = default;
NavigationMesh::~NavigationMesh() = default;

std::expected<void, std::string> NavigationMesh::Load(const std::filesystem::path& path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if (!input)
	{
		return std::unexpected("Cannot open NavMesh file");
	}

	const auto size = input.tellg();
	if (size <= 0 || size > 16 * 1024 * 1024)
	{
		return std::unexpected("NavMesh file size outside 1..16 MiB");
	}

	std::string bytes(static_cast<std::size_t>(size), '\0');
	input.seekg(0);
	if (!input.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
	{
		return std::unexpected("Cannot read NavMesh file");
	}

	boost::system::error_code error;
	boost::json::parse_options parseOptions;
	parseOptions.numbers = boost::json::number_precision::precise;
	auto document = boost::json::parse(bytes, error, {}, parseOptions);
	if (error || !document.is_object())
	{
		return std::unexpected("Invalid NavMesh JSON");
	}

	const auto& root = document.as_object();
	std::string format, coordinates;
	unsigned version{};
	auto next = std::make_unique<State>();
	if (!Text(root, "format", format) || format != "bt-navmesh" || !Field(root, "version", version) || version != 1 ||
	    !Text(root, "coordinates", coordinates) || coordinates != "recast_cm" || !Text(root, "map", next->map))
	{
		return std::unexpected("Unsupported NavMesh format, version or coordinates");
	}

	dtNavMeshParams params{};
	float radius{}, height{};
	unsigned short include{}, exclude{};
	std::vector<float> costs;
	if (!Vector(root, "origin", params.orig) || !Field(root, "tileWidth", params.tileWidth) || !Field(root, "tileHeight", params.tileHeight) ||
	    params.tileWidth <= 0 || params.tileHeight <= 0 || !Field(root, "agentRadius", radius) || radius <= 0 ||
	    !Field(root, "agentHeight", height) || height <= 0 || !Field(root, "includeFlags", include) || !Field(root, "excludeFlags", exclude) ||
	    !Array(root, "areaCosts", costs, DT_MAX_AREAS) || costs.size() != DT_MAX_AREAS)
	{
		return std::unexpected("Invalid NavMesh configuration");
	}

	next->filter.setIncludeFlags(include);
	next->filter.setExcludeFlags(exclude);
	for (int i = 0; i < DT_MAX_AREAS; ++i)
	{
		if (costs[i] < 1)
		{
			return std::unexpected("Area costs must be at least 1");
		}

		next->filter.setAreaCost(i, costs[i]);
	}

	const auto* tiles = root.if_contains("tiles");
	if (!tiles || !tiles->is_array() || tiles->as_array().empty() || tiles->as_array().size() > 8192)
	{
		return std::unexpected("Invalid tile count");
	}

	std::vector<Tile> decoded;
	std::set<std::tuple<int, int, int>> coordinatesSeen;
	int maxPolygons = 0;
	for (const auto& value : tiles->as_array())
	{
		if (!value.is_object())
		{
			return std::unexpected("Tile is not an object");
		}

		auto tile = ReadTile(value.as_object());
		if (!tile)
		{
			return std::unexpected(tile.error());
		}

		const auto& p = tile->params;
		if (!coordinatesSeen.emplace(p.tileX, p.tileY, p.tileLayer).second)
		{
			return std::unexpected("Duplicate tile coordinates");
		}

		maxPolygons = std::max(maxPolygons, p.polyCount);
		decoded.push_back(std::move(*tile));
	}

	params.maxTiles = static_cast<int>(decoded.size());
	params.maxPolys = maxPolygons;
	next->mesh.reset(dtAllocNavMesh());
	if (!next->mesh || dtStatusFailed(next->mesh->init(&params)))
	{
		return std::unexpected("Detour NavMesh initialization failed");
	}

	for (auto& tile : decoded)
	{
		auto& p = tile.params;
		p.nvp = DT_VERTS_PER_POLYGON;
		p.verts = tile.vertices.data();
		p.polys = tile.polygons.data();
		p.polyFlags = tile.flags.data();
		p.polyAreas = tile.areas.data();
		p.detailMeshes = tile.details.data();
		p.detailVerts = tile.detailVertices.data();
		p.detailTris = tile.triangles.data();
		p.walkableRadius = radius;
		p.walkableHeight = height;
		p.buildBvTree = true;
		unsigned char* data = nullptr;
		int dataSize = 0;
		if (!dtCreateNavMeshData(&p, &data, &dataSize))
		{
			return std::unexpected("Detour tile conversion failed");
		}

		const auto status = next->mesh->addTile(data, dataSize, DT_TILE_FREE_DATA, 0, nullptr);
		if (dtStatusFailed(status))
		{
			dtFree(data);
			return std::unexpected("Detour tile loading failed");
		}

		++next->tiles;
		next->polygons += static_cast<unsigned>(p.polyCount);
	}

	unsigned declaredPolygons{};
	if (!Field(root, "polygonCount", declaredPolygons) || declaredPolygons != next->polygons)
	{
		return std::unexpected("Polygon total mismatch");
	}

	state = std::move(next);
	return {};
}

std::expected<std::vector<NavPoint>, std::string> NavigationMesh::FindPath(NavPoint start, NavPoint end) const
{
	if (!state)
	{
		return std::unexpected("No NavMesh loaded");
	}

	const auto a = ToRecast(start), b = ToRecast(end);
	for (int i = 0; i < 3; ++i)
	{
		if (!std::isfinite(a[i]) || !std::isfinite(b[i]))
		{
			return std::unexpected("Non-finite query coordinate");
		}
	}

	auto query = NavigationQuery::Borrow(state->mesh.get());
	if (!query)
	{
		return std::unexpected("Detour query initialization failed or this thread already has a borrowed query");
	}

	dtPolyRef startRef{}, endRef{};
	float projectedStart[3]{}, projectedEnd[3]{};
	if (!Project(*query, state->filter, start, startRef, projectedStart) ||
	    !Project(*query, state->filter, end, endRef, projectedEnd))
	{
		return std::unexpected("Start or destination is outside the NavMesh");
	}

	std::array<dtPolyRef, 8192> corridor{};
	int count{};
	const auto status = query->findPath(startRef, endRef, projectedStart, projectedEnd, &state->filter, corridor.data(), &count, static_cast<int>(corridor.size()));
	if (dtStatusFailed(status) || dtStatusDetail(status, DT_PARTIAL_RESULT | DT_OUT_OF_NODES | DT_BUFFER_TOO_SMALL) || count == 0 || corridor[count - 1] != endRef)
	{
		return std::unexpected("No complete path or search limit exceeded");
	}

	std::array<float, 8192 * 3> straight{};
	std::array<dtPolyRef, 8192> straightRefs{};
	int straightCount{};
	const auto straightStatus = query->findStraightPath(projectedStart, projectedEnd, corridor.data(), count, straight.data(), nullptr, straightRefs.data(),
	                                                    &straightCount, 8192, DT_STRAIGHTPATH_ALL_CROSSINGS);
	if (dtStatusFailed(straightStatus) || dtStatusDetail(straightStatus, DT_BUFFER_TOO_SMALL) || straightCount < 1)
	{
		return std::unexpected("Straight path generation failed or truncated");
	}

	std::vector<NavPoint> points;
	points.reserve(straightCount);
	for (int i = 0; i < straightCount; ++i)
	{
		// Straight-path crossings use coarse polygon heights; movement needs detail-surface heights.
		float height{};
		const dtPolyRef ref = straightRefs[i] != 0 ? straightRefs[i] : endRef;
		if (dtStatusFailed(query->getPolyHeight(ref, &straight[i * 3], &height)))
		{
			float onPoly[3]{};
			if (dtStatusFailed(query->closestPointOnPoly(ref, &straight[i * 3], onPoly, nullptr)))
			{
				return std::unexpected("Could not resolve path crossing height");
			}

			height = onPoly[1];
		}

		straight[i * 3 + 1] = height;
		points.push_back(ToUnreal(&straight[i * 3]));
	}

	points.front() = ToUnreal(projectedStart);
	points.back() = ToUnreal(projectedEnd);
	return points;
}

std::expected<NavPoint, std::string> NavigationMesh::TraceSurface(NavPoint start, NavPoint end) const
{
	if (!state)
	{
		return std::unexpected("No NavMesh loaded");
	}

	auto query = NavigationQuery::Borrow(state->mesh.get());
	if (!query)
	{
		return std::unexpected("Detour query initialization failed or this thread already has a borrowed query");
	}

	dtPolyRef startRef{}, endRef{};
	float a[3]{}, b[3]{};
	if (!Project(*query, state->filter, start, startRef, a) || !Project(*query, state->filter, end, endRef, b))
	{
		return std::unexpected("Surface segment is outside the NavMesh");
	}

	const auto requestedStart = ToRecast(start), requestedEnd = ToRecast(end);
	if (std::hypot(a[0] - requestedStart[0], a[2] - requestedStart[2]) > 0.5f ||
	    std::hypot(b[0] - requestedEnd[0], b[2] - requestedEnd[2]) > 0.5f)
	{
		return std::unexpected("Surface segment would snap across a NavMesh boundary");
	}

	float fraction{}, normal[3]{};
	if (std::hypot(a[0] - b[0], a[2] - b[2]) < 0.001f && std::abs(a[1] - b[1]) < 0.01f)
	{
		return ToUnreal(b);
	}

	std::array<dtPolyRef, 1024> visited{};
	int count{};
	const auto status = query->raycast(startRef, a, b, &state->filter, &fraction, normal, visited.data(), &count, static_cast<int>(visited.size()));
	if (dtStatusFailed(status) || dtStatusDetail(status, DT_BUFFER_TOO_SMALL) || count == 0 || fraction < 1.0f)
	{
		return std::unexpected("Surface segment crosses a wall or gap");
	}

	float height{};
	if (dtStatusFailed(query->getPolyHeight(visited[count - 1], b, &height)))
	{
		float onPoly[3]{};
		if (dtStatusFailed(query->closestPointOnPoly(visited[count - 1], b, onPoly, nullptr)) ||
		    std::hypot(onPoly[0] - b[0], onPoly[2] - b[2]) > 0.5f)
		{
			return std::unexpected("Surface endpoint is outside the visited polygon");
		}

		height = onPoly[1];
	}

	if (std::abs(height - b[1]) > 2.0f)
	{
		return std::unexpected("Surface segment reaches another floor");
	}

	b[1] = height;
	return ToUnreal(b);
}

const std::string& NavigationMesh::MapName() const
{
	static const std::string empty;
	return state ? state->map : empty;
}

std::expected<NavPoint, std::string> NavigationMesh::ProjectPoint(NavPoint point) const
{
	if (!state)
	{
		return std::unexpected("No NavMesh loaded");
	}

	const auto query = NavigationQuery::Borrow(state->mesh.get());
	if (!query)
	{
		return std::unexpected("Detour query initialization failed or this thread already has a borrowed query");
	}

	float projected[3]{};
	if (dtPolyRef ref{}; !Project(*query, state->filter, point, ref, projected))
	{
		return std::unexpected("Point is outside the NavMesh projection tolerance");
	}

	return ToUnreal(projected);
}

std::expected<NavPoint, std::string> NavigationMesh::RandomPoint(std::mt19937& random) const
{
	if (!state)
	{
		return std::unexpected("No NavMesh loaded");
	}

	auto query = NavigationQuery::Borrow(state->mesh.get());
	if (!query)
	{
		return std::unexpected("Detour query initialization failed or this thread already has a borrowed query");
	}

	RandomScope scope(random);
	dtPolyRef ref{};
	float point[3]{};
	if (dtStatusFailed(query->findRandomPoint(&state->filter, RandomFraction, &ref, point)) || !ref)
	{
		return std::unexpected("No walkable polygon for random placement");
	}

	return ToUnreal(point);
}

std::expected<NavPoint, std::string> NavigationMesh::RandomReachablePoint(NavPoint center, float radius, std::mt19937& random) const
{
	if (!state)
	{
		return std::unexpected("No NavMesh loaded");
	}

	if (!std::isfinite(radius) || radius <= 0 || radius > 100000)
	{
		return std::unexpected("Spawn radius must be in (0, 100000] cm");
	}

	auto query = NavigationQuery::Borrow(state->mesh.get());
	if (!query)
	{
		return std::unexpected("Detour query initialization failed or this thread already has a borrowed query");
	}

	dtPolyRef startRef{};
	float projected[3]{};
	if (!Project(*query, state->filter, center, startRef, projected))
	{
		return std::unexpected("Spawn center is outside the NavMesh");
	}

	RandomScope scope(random);
	for (unsigned attempt = 0; attempt < 24; ++attempt)
	{
		dtPolyRef ref{};
		float point[3]{};
		const auto status = query->findRandomPointAroundCircle(startRef, projected, radius, &state->filter, RandomFraction, &ref, point);
		if (dtStatusFailed(status) || dtStatusDetail(status, DT_OUT_OF_NODES | DT_PARTIAL_RESULT) || !ref)
		{
			return std::unexpected("Reachable spawn query failed or exceeded its search limit");
		}

		const auto result = ToUnreal(point);
		const double dx = static_cast<double>(result.x) - center.x, dy = static_cast<double>(result.y) - center.y;
		if (dx * dx + dy * dy <= static_cast<double>(radius) * radius)
		{
			return result;
		}
	}

	return std::unexpected("No reachable point sampled inside the strict spawn radius");
}

unsigned NavigationMesh::TileCount() const
{
	return state ? state->tiles : 0;
}

unsigned NavigationMesh::PolygonCount() const
{
	return state ? state->polygons : 0;
}
