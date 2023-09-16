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

#define NOGDI
#include <xbyak\xbyak.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>

namespace DebugAPI_IMPL
{
	class DebugAPILine
	{
	public:
		DebugAPILine(glm::vec3 from, glm::vec3 to, glm::vec4 color, float lineThickness, unsigned __int64 destroyTickCount);

		glm::vec3 From;
		glm::vec3 To;
		glm::vec4 Color;
		float fColor;
		float Alpha;
		float LineThickness;

		unsigned __int64 DestroyTickCount;
	};

	class DebugAPI
	{
	public:
		static void Update();

		static RE::GPtr<RE::IMenu> GetHUD();

		static void DrawLine2D(RE::GPtr<RE::GFxMovieView> movie, glm::vec2 from, glm::vec2 to, float color, float lineThickness,
			float alpha);
		static void DrawLine2D(RE::GPtr<RE::GFxMovieView> movie, glm::vec2 from, glm::vec2 to, glm::vec4 color,
			float lineThickness);
		static void DrawLine3D(RE::GPtr<RE::GFxMovieView> movie, glm::vec3 from, glm::vec3 to, float color, float lineThickness,
			float alpha);
		static void DrawLine3D(RE::GPtr<RE::GFxMovieView> movie, glm::vec3 from, glm::vec3 to, glm::vec4 color,
			float lineThickness);
		static void ClearLines2D(RE::GPtr<RE::GFxMovieView> movie);

		static void DrawLineForMS(const glm::vec3& from, const glm::vec3& to, int liftetimeMS = 10,
			const glm::vec4& color = { 1.0f, 0.0f, 0.0f, 1.0f }, float lineThickness = 1);
		static void DrawSphere(glm::vec3, float radius, int liftetimeMS = 10, const glm::vec4& color = { 1.0f, 0.0f, 0.0f, 1.0f },
			float lineThickness = 1);
		static void DrawCircle(glm::vec3, float radius, glm::vec3 eulerAngles, int liftetimeMS = 10,
			const glm::vec4& color = { 1.0f, 0.0f, 0.0f, 1.0f }, float lineThickness = 1);

		static std::mutex LinesToDraw_mutex;
		static std::vector<DebugAPILine*> LinesToDraw;

		static bool DEBUG_API_REGISTERED;

		static constexpr int CIRCLE_NUM_SEGMENTS = 32;

		static constexpr float DRAW_LOC_MAX_DIF = 5.0f;

		static glm::vec2 WorldToScreenLoc(RE::GPtr<RE::GFxMovieView> movie, glm::vec3 worldLoc);
		static float RGBToHex(glm::vec3 rgb);

		static void FastClampToScreen(glm::vec2& point);

		// 	static void ClampVectorToScreen(glm::vec2& from, glm::vec2& to);
		// 	static void ClampPointToScreen(glm::vec2& point, float lineAngle);

		static bool IsOnScreen(glm::vec2 from, glm::vec2 to);
		static bool IsOnScreen(glm::vec2 point);

		static void CacheMenuData();

		static bool CachedMenuData;

		static float ScreenResX;
		static float ScreenResY;

	private:
		static float ConvertComponentR(float value);
		static float ConvertComponentG(float value);
		static float ConvertComponentB(float value);
		// returns true if there is already a line with the same color at around the same from and to position
		// with some leniency to bundle together lines in roughly the same spot (see DRAW_LOC_MAX_DIF)
		static DebugAPILine* GetExistingLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color,
			float lineThickness);
	};

	class DebugOverlayMenu : RE::IMenu
	{
	public:
		static constexpr const char* MENU_PATH = "BetterThirdPersonSelection/overlay_menu";
		static constexpr const char* MENU_NAME = "HUD Menu";

		DebugOverlayMenu();

		static void Register();

		static void Show();
		static void Hide();

		static RE::stl::owner<RE::IMenu*> Creator() { return new DebugOverlayMenu(); }

		void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override;

	private:
		class Logger : public RE::GFxLog
		{
		public:
			void LogMessageVarg(LogMessageType, const char* a_fmt, std::va_list a_argList) override
			{
				std::string fmt(a_fmt ? a_fmt : "");
				while (!fmt.empty() && fmt.back() == '\n') {
					fmt.pop_back();
				}

				std::va_list args;
				va_copy(args, a_argList);
				std::vector<char> buf(static_cast<std::size_t>(std::vsnprintf(0, 0, fmt.c_str(), a_argList) + 1));
				std::vsnprintf(buf.data(), buf.size(), fmt.c_str(), args);
				va_end(args);

				logger::info("{}"sv, buf.data());
			}
		};
	};

	namespace DrawDebug
	{
		namespace Colors
		{
			static constexpr glm::vec4 RED = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
			static constexpr glm::vec4 GRN = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
			static constexpr glm::vec4 BLU = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
		}

		template <glm::vec4 Color = Colors::RED>
		void draw_line(const RE::NiPoint3& _from, const RE::NiPoint3& _to, float size = 5.0f, int time = 3000)
		{
			glm::vec3 from(_from.x, _from.y, _from.z);
			glm::vec3 to(_to.x, _to.y, _to.z);
			DebugAPI::DrawLineForMS(from, to, time, Color, size);
		}

		template <glm::vec4 Color = Colors::RED>
		void draw_line0(const RE::NiPoint3& _from, const RE::NiPoint3& _to, float size = 5.0f)
		{
			return draw_line<Color>(_from, _to, size, 0);
		}

		template <glm::vec4 Color = Colors::RED>
		void draw_point(const RE::NiPoint3& _pos, float size = 5.0f, int time = 3000)
		{
			glm::vec3 from(_pos.x, _pos.y, _pos.z);
			glm::vec3 to(_pos.x, _pos.y, _pos.z + 5);
			DebugAPI::DrawLineForMS(from, to, time, Color, size);
		}

		template <glm::vec4 Color = Colors::RED>
		void draw_point0(const RE::NiPoint3& _pos, float size = 5.0f)
		{
			return draw_point<Color>(_pos, size, 0);
		}

		template <glm::vec4 Color = Colors::RED>
		void draw_sphere(const RE::NiPoint3& _center, float r = 5.0f, float size = 5.0f, int time = 3000)
		{
			glm::vec3 center(_center.x, _center.y, _center.z);
			DebugAPI::DrawSphere(center, r, time, Color, size);
		}
	}
}
using namespace DebugAPI_IMPL::DrawDebug;

namespace DebugAPI_IMPL
{
	glm::highp_mat4 GetRotationMatrix(glm::vec3 eulerAngles)
	{
		return glm::eulerAngleXYZ(-(eulerAngles.x), -(eulerAngles.y), -(eulerAngles.z));
	}

	glm::vec3 NormalizeVector(glm::vec3 p) { return glm::normalize(p); }

	glm::vec3 RotateVector(glm::quat quatIn, glm::vec3 vecIn)
	{
		float num = quatIn.x * 2.0f;
		float num2 = quatIn.y * 2.0f;
		float num3 = quatIn.z * 2.0f;
		float num4 = quatIn.x * num;
		float num5 = quatIn.y * num2;
		float num6 = quatIn.z * num3;
		float num7 = quatIn.x * num2;
		float num8 = quatIn.x * num3;
		float num9 = quatIn.y * num3;
		float num10 = quatIn.w * num;
		float num11 = quatIn.w * num2;
		float num12 = quatIn.w * num3;
		glm::vec3 result;
		result.x = (1.0f - (num5 + num6)) * vecIn.x + (num7 - num12) * vecIn.y + (num8 + num11) * vecIn.z;
		result.y = (num7 + num12) * vecIn.x + (1.0f - (num4 + num6)) * vecIn.y + (num9 - num10) * vecIn.z;
		result.z = (num8 - num11) * vecIn.x + (num9 + num10) * vecIn.y + (1.0f - (num4 + num5)) * vecIn.z;
		return result;
	}

	glm::vec3 RotateVector(glm::vec3 eulerIn, glm::vec3 vecIn)
	{
		glm::vec3 glmVecIn(vecIn.x, vecIn.y, vecIn.z);
		glm::mat3 rotationMatrix = glm::eulerAngleXYZ(eulerIn.x, eulerIn.y, eulerIn.z);

		return rotationMatrix * glmVecIn;
	}

	glm::vec3 GetForwardVector(glm::quat quatIn)
	{
		// rotate Skyrim's base forward vector (positive Y forward) by quaternion
		return RotateVector(quatIn, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	glm::vec3 GetForwardVector(glm::vec3 eulerIn)
	{
		float pitch = eulerIn.x;
		float yaw = eulerIn.z;

		return glm::vec3(sin(yaw) * cos(pitch), cos(yaw) * cos(pitch), sin(pitch));
	}

	glm::vec3 GetRightVector(glm::quat quatIn)
	{
		// rotate Skyrim's base right vector (positive X forward) by quaternion
		return RotateVector(quatIn, glm::vec3(1.0f, 0.0f, 0.0f));
	}

	glm::vec3 GetRightVector(glm::vec3 eulerIn)
	{
		float pitch = eulerIn.x;
		float yaw = eulerIn.z + glm::half_pi<float>();

		return glm::vec3(sin(yaw) * cos(pitch), cos(yaw) * cos(pitch), sin(pitch));
	}

	glm::vec3 ThreeAxisRotation(float r11, float r12, float r21, float r31, float r32)
	{
		return glm::vec3(asin(r21), atan2(r11, r12), atan2(-r31, r32));
	}

	glm::vec3 RotMatrixToEuler(RE::NiMatrix3 matrixIn)
	{
		auto ent = matrixIn.entry;
		auto rotMat =
			glm::mat4({ ent[0][0], ent[1][0], ent[2][0], ent[0][1], ent[1][1], ent[2][1], ent[0][2], ent[1][2], ent[2][2] });

		glm::vec3 rotOut;
		glm::extractEulerAngleXYZ(rotMat, rotOut.x, rotOut.y, rotOut.z);

		return rotOut;
	}

	constexpr int FIND_COLLISION_MAX_RECURSION = 2;

	RE::NiAVObject* GetCharacterSpine(RE::TESObjectREFR* object)
	{
		auto characterObject = object->GetObjectReference()->As<RE::TESNPC>();
		auto mesh = object->GetCurrent3D();

		if (characterObject && mesh) {
			auto spineNode = mesh->GetObjectByName("NPC Spine [Spn0]");
			if (spineNode)
				return spineNode;
		}

		return mesh;
	}

	RE::NiAVObject* GetCharacterHead(RE::TESObjectREFR* object)
	{
		auto characterObject = object->GetObjectReference()->As<RE::TESNPC>();
		auto mesh = object->GetCurrent3D();

		if (characterObject && mesh) {
			auto spineNode = mesh->GetObjectByName("NPC Head [Head]");
			if (spineNode)
				return spineNode;
		}

		return mesh;
	}

	bool IsRoughlyEqual(float first, float second, float maxDif) { return abs(first - second) <= maxDif; }

	glm::vec3 QuatToEuler(glm::quat q)
	{
		auto matrix = glm::toMat4(q);

		glm::vec3 rotOut;
		glm::extractEulerAngleXYZ(matrix, rotOut.x, rotOut.y, rotOut.z);

		return rotOut;
	}

	glm::quat EulerToQuat(glm::vec3 rotIn)
	{
		auto matrix = glm::eulerAngleXYZ(rotIn.x, rotIn.y, rotIn.z);
		return glm::toQuat(matrix);
	}

	glm::vec3 GetInverseRotation(glm::vec3 rotIn)
	{
		auto matrix = glm::eulerAngleXYZ(rotIn.y, rotIn.x, rotIn.z);
		auto inverseMatrix = glm::inverse(matrix);

		glm::vec3 rotOut;
		glm::extractEulerAngleYXZ(inverseMatrix, rotOut.x, rotOut.y, rotOut.z);
		return rotOut;
	}

	glm::quat GetInverseRotation(glm::quat rotIn) { return glm::inverse(rotIn); }

	glm::vec3 EulerRotationToVector(glm::vec3 rotIn)
	{
		return glm::vec3(cos(rotIn.y) * cos(rotIn.x), sin(rotIn.y) * cos(rotIn.x), sin(rotIn.x));
	}

	glm::vec3 VectorToEulerRotation(glm::vec3 vecIn)
	{
		float yaw = atan2(vecIn.x, vecIn.y);
		float pitch = atan2(vecIn.z, sqrt((vecIn.x * vecIn.x) + (vecIn.y * vecIn.y)));

		return glm::vec3(pitch, 0.0f, yaw);
	}

	glm::vec3 GetCameraPos()
	{
		auto playerCam = RE::PlayerCamera::GetSingleton();
		return glm::vec3(playerCam->pos.x, playerCam->pos.y, playerCam->pos.z);
	}

	glm::quat GetCameraRot()
	{
		auto playerCam = RE::PlayerCamera::GetSingleton();

		auto cameraState = playerCam->currentState.get();
		if (!cameraState)
			return glm::quat();

		RE::NiQuaternion niRotation;
		cameraState->GetRotation(niRotation);

		return glm::quat(niRotation.w, niRotation.x, niRotation.y, niRotation.z);
	}

	bool IsPosBehindPlayerCamera(glm::vec3 pos)
	{
		auto cameraPos = GetCameraPos();
		auto cameraRot = GetCameraRot();

		auto toTarget = NormalizeVector(pos - cameraPos);
		auto cameraForward = NormalizeVector(GetForwardVector(cameraRot));

		auto angleDif = abs(glm::length(toTarget - cameraForward));

		// root_two is the diagonal length of a 1x1 square. When comparing normalized forward
		// vectors, this accepts an angle of 90 degrees in all directions
		return angleDif > glm::root_two<float>();
	}

	glm::vec3 GetPointOnRotatedCircle(glm::vec3 origin, float radius, float i, float maxI, glm::vec3 eulerAngles)
	{
		float currAngle = (i / maxI) * glm::two_pi<float>();

		glm::vec3 targetPos((radius * cos(currAngle)), (radius * sin(currAngle)), 0.0f);

		auto targetPosRotated = RotateVector(eulerAngles, targetPos);

		return glm::vec3(targetPosRotated.x + origin.x, targetPosRotated.y + origin.y, targetPosRotated.z + origin.z);
	}

	glm::vec3 GetObjectAccuratePosition(RE::TESObjectREFR* object)
	{
		auto mesh = object->GetCurrent3D();

		// backup, if no mesh is found
		if (!mesh) {
			auto niPos = object->GetPosition();
			return glm::vec3(niPos.x, niPos.y, niPos.z);
		}

		auto niPos = mesh->world.translate;
		return glm::vec3(niPos.x, niPos.y, niPos.z);
	}

	std::mutex DebugAPI::LinesToDraw_mutex;
	std::vector<DebugAPILine*> DebugAPI::LinesToDraw;

	bool DebugAPI::CachedMenuData;

	float DebugAPI::ScreenResX;
	float DebugAPI::ScreenResY;

	DebugAPILine::DebugAPILine(glm::vec3 from, glm::vec3 to, glm::vec4 color, float lineThickness,
		unsigned __int64 destroyTickCount)
	{
		From = from;
		To = to;
		Color = color;
		fColor = DebugAPI::RGBToHex(color);
		Alpha = color.a * 100.0f;
		LineThickness = lineThickness;
		DestroyTickCount = destroyTickCount;
	}

	void DebugAPI::DrawLineForMS(const glm::vec3& from, const glm::vec3& to, int liftetimeMS, const glm::vec4& color,
		float lineThickness)
	{
		DebugAPILine* oldLine = GetExistingLine(from, to, color, lineThickness);
		if (oldLine) {
			oldLine->From = from;
			oldLine->To = to;
			oldLine->DestroyTickCount = GetTickCount64() + liftetimeMS;
			oldLine->LineThickness = lineThickness;
			return;
		}

		DebugAPILine* newLine = new DebugAPILine(from, to, color, lineThickness, GetTickCount64() + liftetimeMS);
		std::lock_guard<std::mutex> lg(LinesToDraw_mutex);
		LinesToDraw.push_back(newLine);
	}

	void DebugAPI::Update()
	{
		auto hud = GetHUD();
		if (!hud || !hud->uiMovie)
			return;

		CacheMenuData();
		ClearLines2D(hud->uiMovie);

		std::lock_guard<std::mutex> lg(LinesToDraw_mutex);
		for (int i = 0; i < LinesToDraw.size(); i++) {
			DebugAPILine* line = LinesToDraw[i];

			DrawLine3D(hud->uiMovie, line->From, line->To, line->fColor, line->LineThickness, line->Alpha);

			if (GetTickCount64() > line->DestroyTickCount) {
				LinesToDraw.erase(LinesToDraw.begin() + i);
				delete line;

				i--;
				continue;
			}
		}
	}

	void DebugAPI::DrawSphere(glm::vec3 origin, float radius, int liftetimeMS, const glm::vec4& color, float lineThickness)
	{
		DrawCircle(origin, radius, glm::vec3(0.0f, 0.0f, 0.0f), liftetimeMS, color, lineThickness);
		DrawCircle(origin, radius, glm::vec3(glm::half_pi<float>(), 0.0f, 0.0f), liftetimeMS, color, lineThickness);
	}

	void DebugAPI::DrawCircle(glm::vec3 origin, float radius, glm::vec3 eulerAngles, int liftetimeMS, const glm::vec4& color,
		float lineThickness)
	{
		glm::vec3 lastEndPos =
			GetPointOnRotatedCircle(origin, radius, CIRCLE_NUM_SEGMENTS, (float)(CIRCLE_NUM_SEGMENTS - 1), eulerAngles);

		for (int i = 0; i <= CIRCLE_NUM_SEGMENTS; i++) {
			glm::vec3 currEndPos =
				GetPointOnRotatedCircle(origin, radius, (float)i, (float)(CIRCLE_NUM_SEGMENTS - 1), eulerAngles);

			DrawLineForMS(lastEndPos, currEndPos, liftetimeMS, color, lineThickness);

			lastEndPos = currEndPos;
		}
	}

	DebugAPILine* DebugAPI::GetExistingLine(const glm::vec3& from, const glm::vec3& to, const glm::vec4& color,
		float lineThickness)
	{
		std::lock_guard<std::mutex> lg(LinesToDraw_mutex);
		for (int i = 0; i < LinesToDraw.size(); i++) {
			DebugAPILine* line = LinesToDraw[i];

			if (IsRoughlyEqual(from.x, line->From.x, DRAW_LOC_MAX_DIF) &&
				IsRoughlyEqual(from.y, line->From.y, DRAW_LOC_MAX_DIF) &&
				IsRoughlyEqual(from.z, line->From.z, DRAW_LOC_MAX_DIF) && IsRoughlyEqual(to.x, line->To.x, DRAW_LOC_MAX_DIF) &&
				IsRoughlyEqual(to.y, line->To.y, DRAW_LOC_MAX_DIF) && IsRoughlyEqual(to.z, line->To.z, DRAW_LOC_MAX_DIF) &&
				IsRoughlyEqual(lineThickness, line->LineThickness, DRAW_LOC_MAX_DIF) && color == line->Color) {
				return line;
			}
		}

		return nullptr;
	}

	void DebugAPI::DrawLine3D(RE::GPtr<RE::GFxMovieView> movie, glm::vec3 from, glm::vec3 to, float color, float lineThickness,
		float alpha)
	{
		if (IsPosBehindPlayerCamera(from) && IsPosBehindPlayerCamera(to))
			return;

		glm::vec2 screenLocFrom = WorldToScreenLoc(movie, from);
		glm::vec2 screenLocTo = WorldToScreenLoc(movie, to);
		DrawLine2D(movie, screenLocFrom, screenLocTo, color, lineThickness, alpha);
	}

	void DebugAPI::DrawLine3D(RE::GPtr<RE::GFxMovieView> movie, glm::vec3 from, glm::vec3 to, glm::vec4 color,
		float lineThickness)
	{
		DrawLine3D(movie, from, to, RGBToHex(glm::vec3(color.r, color.g, color.b)), lineThickness, color.a * 100.0f);
	}

	void DebugAPI::DrawLine2D(RE::GPtr<RE::GFxMovieView> movie, glm::vec2 from, glm::vec2 to, float color, float lineThickness,
		float alpha)
	{
		// all parts of the line are off screen - don't need to draw them
		if (!IsOnScreen(from, to))
			return;

		FastClampToScreen(from);
		FastClampToScreen(to);

		// lineStyle(thickness:Number = NaN, color : uint = 0, alpha : Number = 1.0, pixelHinting : Boolean = false,
		// scaleMode : String = "normal", caps : String = null, joints : String = null, miterLimit : Number = 3) :void
		//
		// CapsStyle values: 'NONE', 'ROUND', 'SQUARE'
		// const char* capsStyle = "NONE";

		RE::GFxValue argsLineStyle[3]{ lineThickness, color, alpha };
		movie->Invoke("lineStyle", nullptr, argsLineStyle, 3);

		RE::GFxValue argsStartPos[2]{ from.x, from.y };
		movie->Invoke("moveTo", nullptr, argsStartPos, 2);

		RE::GFxValue argsEndPos[2]{ to.x, to.y };
		movie->Invoke("lineTo", nullptr, argsEndPos, 2);

		movie->Invoke("endFill", nullptr, nullptr, 0);
	}

	void DebugAPI::DrawLine2D(RE::GPtr<RE::GFxMovieView> movie, glm::vec2 from, glm::vec2 to, glm::vec4 color,
		float lineThickness)
	{
		DrawLine2D(movie, from, to, RGBToHex(glm::vec3(color.r, color.g, color.b)), lineThickness, color.a * 100.0f);
	}

	void DebugAPI::ClearLines2D(RE::GPtr<RE::GFxMovieView> movie) { movie->Invoke("clear", nullptr, nullptr, 0); }

	RE::GPtr<RE::IMenu> DebugAPI::GetHUD()
	{
		RE::GPtr<RE::IMenu> hud = RE::UI::GetSingleton()->GetMenu(DebugOverlayMenu::MENU_NAME);
		return hud;
	}

	float DebugAPI::RGBToHex(glm::vec3 rgb)
	{
		return ConvertComponentR(rgb.r * 255) + ConvertComponentG(rgb.g * 255) + ConvertComponentB(rgb.b * 255);
	}

	// if drawing outside the screen rect, at some point Scaleform seems to start resizing the rect internally, without
	// increasing resolution. This will cause all line draws to become more and more pixelated and increase in thickness
	// the farther off screen even one line draw goes. I'm allowing some leeway, then I just clamp the
	// coordinates to the screen rect.
	//
	// this is inaccurate. A more accurate solution would require finding the sub vector that overshoots the screen rect between
	// two points and scale the vector accordingly. Might implement that at some point, but the inaccuracy is barely noticeable
	const float CLAMP_MAX_OVERSHOOT = 10000.0f;
	void DebugAPI::FastClampToScreen(glm::vec2& point)
	{
		if (point.x < 0.0) {
			float overshootX = abs(point.x);
			if (overshootX > CLAMP_MAX_OVERSHOOT)
				point.x += overshootX - CLAMP_MAX_OVERSHOOT;
		} else if (point.x > ScreenResX) {
			float overshootX = point.x - ScreenResX;
			if (overshootX > CLAMP_MAX_OVERSHOOT)
				point.x -= overshootX - CLAMP_MAX_OVERSHOOT;
		}

		if (point.y < 0.0) {
			float overshootY = abs(point.y);
			if (overshootY > CLAMP_MAX_OVERSHOOT)
				point.y += overshootY - CLAMP_MAX_OVERSHOOT;
		} else if (point.y > ScreenResY) {
			float overshootY = point.y - ScreenResY;
			if (overshootY > CLAMP_MAX_OVERSHOOT)
				point.y -= overshootY - CLAMP_MAX_OVERSHOOT;
		}
	}

	float DebugAPI::ConvertComponentR(float value) { return (value * 0xffff) + value; }

	float DebugAPI::ConvertComponentG(float value) { return (value * 0xff) + value; }

	float DebugAPI::ConvertComponentB(float value) { return value; }

	glm::vec2 DebugAPI::WorldToScreenLoc(RE::GPtr<RE::GFxMovieView> movie, glm::vec3 worldLoc)
	{
		glm::vec2 screenLocOut;
		RE::NiPoint3 niWorldLoc(worldLoc.x, worldLoc.y, worldLoc.z);

		float zVal;

		RE::NiCamera::WorldPtToScreenPt3((float(*)[4])(REL::ID(519579).address()),
			*((RE::NiRect<float>*)REL::ID(519618).address()), niWorldLoc, screenLocOut.x, screenLocOut.y, zVal, 1e-5f);
		RE::GRectF rect = movie->GetVisibleFrameRect();

		screenLocOut.x = rect.left + (rect.right - rect.left) * screenLocOut.x;
		screenLocOut.y = 1.0f - screenLocOut.y;  // Flip y for Flash coordinate system
		screenLocOut.y = rect.top + (rect.bottom - rect.top) * screenLocOut.y;

		return screenLocOut;
	}

	DebugOverlayMenu::DebugOverlayMenu()
	{
		auto scaleformManager = RE::BSScaleformManager::GetSingleton();

		inputContext = Context::kNone;
		depthPriority = 127;

		menuFlags.set(RE::UI_MENU_FLAGS::kRequiresUpdate);
		menuFlags.set(RE::UI_MENU_FLAGS::kAllowSaving);
		menuFlags.set(RE::UI_MENU_FLAGS::kCustomRendering);

		scaleformManager->LoadMovieEx(this, MENU_PATH, [](RE::GFxMovieDef* a_def) -> void {
			a_def->SetState(RE::GFxState::StateType::kLog, RE::make_gptr<Logger>().get());
		});
	}

	void DebugOverlayMenu::Register()
	{
		auto ui = RE::UI::GetSingleton();
		if (ui) {
			ui->Register(MENU_NAME, Creator);

			DebugOverlayMenu::Show();
		}
	}

	void DebugOverlayMenu::Show()
	{
		auto msgQ = RE::UIMessageQueue::GetSingleton();
		if (msgQ) {
			msgQ->AddMessage(DebugOverlayMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kShow, nullptr);
		}
	}

	void DebugOverlayMenu::Hide()
	{
		auto msgQ = RE::UIMessageQueue::GetSingleton();
		if (msgQ) {
			msgQ->AddMessage(DebugOverlayMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
		}
	}

	void DebugAPI::CacheMenuData()
	{
		if (CachedMenuData)
			return;

		RE::GPtr<RE::IMenu> menu = RE::UI::GetSingleton()->GetMenu(DebugOverlayMenu::MENU_NAME);
		if (!menu || !menu->uiMovie)
			return;

		RE::GRectF rect = menu->uiMovie->GetVisibleFrameRect();

		ScreenResX = abs(rect.left - rect.right);
		ScreenResY = abs(rect.top - rect.bottom);

		CachedMenuData = true;
	}

	bool DebugAPI::IsOnScreen(glm::vec2 from, glm::vec2 to) { return IsOnScreen(from) || IsOnScreen(to); }

	bool DebugAPI::IsOnScreen(glm::vec2 point)
	{
		return (point.x <= ScreenResX && point.x >= 0.0 && point.y <= ScreenResY && point.y >= 0.0);
	}

	void DebugOverlayMenu::AdvanceMovie(float a_interval, std::uint32_t a_currentTime)
	{
		RE::IMenu::AdvanceMovie(a_interval, a_currentTime);

		DebugAPI::Update();
	}
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
				auto mid = point0 + point1 + point2;
				mid *= 1.0f / 3.0f;

				draw_line<Colors::GRN>(point0, mid, 3, 0);
				draw_line<Colors::GRN>(point1, mid, 3, 0);
				draw_line<Colors::GRN>(point2, mid, 3, 0);
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
		//SKSE::GetTaskInterface()->AddUITask([]() { DebugAPI_IMPL::DebugAPI::Update(); });
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
