extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

static const inline RE::BSFixedString path = "marker_light.nif";

void change_model(RE::TESObjectLIGH* ligh)
{
	ligh->model = path;
}

void change_models() {
	auto& a = RE::TESDataHandler::GetSingleton()->formArrays[static_cast<int>(RE::FormType::Light)];
	
	for (auto& _i : a) { auto i = _i->As<RE::TESObjectLIGH>();
		if (!i || i->FORMTYPE != RE::FormType::Light)
			continue;

		if (!i->model.data() || !i->model.length()) {
			change_model(i);
		}
	}
}

void draw_navmeshes()
{
	if (auto _navmeshes = RE::PlayerCharacter::GetSingleton()->GetParentCell()->navMeshes) {
		const auto& navmeshes = _navmeshes->navMeshes;
		for (auto& _navmesh : navmeshes) {
			auto navmesh = _navmesh.get();
			auto& vertices = navmesh->vertices;

			for (auto& vertex : vertices) {
				draw_point<Colors::BLU>(vertex.location, 5, 0);
			}

			for (auto& triangle : navmesh->triangles) {
				auto point0 = vertices[triangle.vertices[0]].location;
				auto point1 = vertices[triangle.vertices[1]].location;
				auto point2 = vertices[triangle.vertices[2]].location;
				draw_line(point0, point1, 3, 0);
				draw_line(point0, point2, 3, 0);
				draw_line(point2, point1, 3, 0);
			}
		}
	}
}

class DebugAPIHook
{
public:
	static void Hook() { _Update = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_PlayerCharacter[0])).write_vfunc(0xad, Update); }

private:
	static void Update(RE::PlayerCharacter* a, float delta)
	{
		_Update(a, delta);
		draw_navmeshes();
		DebugAPI_IMPL::DebugAPI::Update();
	}

	static inline REL::Relocation<decltype(Update)> _Update;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		change_models();

		DebugAPIHook::Hook();

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
