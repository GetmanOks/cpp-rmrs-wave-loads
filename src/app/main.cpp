#include "ApplicabilityChecker.h"
#include "WaveLoadCalculator.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

// Пытаемся загрузить системный шрифт с поддержкой кириллицы
void loadCyrillicFont(ImGuiIO& io) {
	const char* candidates[] = {
		"C:/Windows/Fonts/segoeui.ttf",
		"C:/Windows/Fonts/arial.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
	};
	for (const char* path : candidates) {
		if (FILE* f = std::fopen(path, "rb")) {
			std::fclose(f);
			io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr,
				io.Fonts->GetGlyphRangesCyrillic());
			return;
		}
	}
}

void glfwErrorCallback(int error, const char* description) {
	std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

void sectionHeader(const char* label) {
	ImGui::Spacing();
	ImGui::TextUnformatted(label);
	ImGui::Separator();
}

// Список причин
void renderReasons(const std::vector<std::string>& reasons) {
	for (const std::string& r : reasons) {
		ImGui::Bullet();
		ImGui::TextWrapped("%s", r.c_str());
	}
}

// Одна кривая на эпюре
struct PlotSeries {
	const char*        name;
	ImU32              color;
	ImU32              fill;
	std::vector<float> values;
};

ImU32 withAlpha(ImU32 color, int alpha) {
	return (color & 0x00FFFFFF) | (static_cast<ImU32>(alpha) << 24);
}

// Линейная интерполяция значения кривой в точке xVal
float interpolateAt(const std::vector<float>& xs, const std::vector<float>& ys, float xVal) {
	if (xs.empty() || ys.empty()) {
		return 0.0f;
	}
	const size_t n = std::min(xs.size(), ys.size());
	if (xVal <= xs.front()) {
		return ys.front();
	}
	if (xVal >= xs[n - 1]) {
		return ys[n - 1];
	}
	for (size_t i = 0; i + 1 < n; ++i) {
		if (xVal <= xs[i + 1]) {
			const float span = xs[i + 1] - xs[i];
			const float t = (span > 0.0f) ? (xVal - xs[i]) / span : 0.0f;
			return ys[i] + t * (ys[i + 1] - ys[i]);
		}
	}
	return ys[n - 1];
}

// Рисует эпюру (легенда и подсказка по курсору)
// x - координаты сечений (м), length - длина судна L
void drawEpure(const char* title,
               const std::vector<float>& x,
               const std::vector<PlotSeries>& series,
               float length,
               float height = 220.0f) {
	ImGui::TextUnformatted(title);

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	float width = ImGui::GetContentRegionAvail().x;
	width = std::max(width, 160.0f);
	const ImVec2 size(width, height);

	ImGui::InvisibleButton(title, size);
	const bool hovered = ImGui::IsItemHovered();
	const ImVec2 br(origin.x + size.x, origin.y + size.y);

	dl->AddRectFilled(origin, br, IM_COL32(22, 24, 28, 255));
	dl->AddRect(origin, br, IM_COL32(90, 90, 100, 255));

	float yMin = 0.0f;
	float yMax = 0.0f;
	for (const PlotSeries& s : series) {
		for (float v : s.values) {
			yMin = std::min(yMin, v);
			yMax = std::max(yMax, v);
		}
	}
	if (yMax == yMin) {
		yMax += 1.0f;
		yMin -= 1.0f;
	}
	const float margin = (yMax - yMin) * 0.08f;
	yMax += margin;
	yMin -= margin;

	char yMaxBuf[80];
	char yMinBuf[80];
	std::snprintf(yMaxBuf, sizeof(yMaxBuf), "%.0f", yMax);
	std::snprintf(yMinBuf, sizeof(yMinBuf), "%.0f", yMin);
	const float labelW = std::max(ImGui::CalcTextSize(yMaxBuf).x, ImGui::CalcTextSize(yMinBuf).x);
	const float textH = ImGui::GetTextLineHeight();
	const float padL = std::max(56.0f, labelW + 12.0f);
	const float padR = 16.0f;
	const float padT = 6.0f;
	const float padB = 24.0f;
	const ImVec2 tl(origin.x + padL, origin.y + padT);
	const ImVec2 rb(br.x - padR, br.y - padB);

	const float safeLen = (length > 0.0f) ? length : 1.0f;
	auto mapX = [&](float xv) {
		return tl.x + (rb.x - tl.x) * (xv / safeLen);
	};
	auto mapY = [&](float yv) {
		return rb.y - (rb.y - tl.y) * ((yv - yMin) / (yMax - yMin));
	};

	const ImU32 axisColor = IM_COL32(120, 120, 130, 255);
	const ImU32 gridColor = IM_COL32(70, 72, 80, 90);
	const ImU32 textColor = IM_COL32(190, 190, 200, 255);

	// Характерные сечения из табл. 1.4.4.1 / 1.4.4.2
	const float gridRel[] = { 0.0f, 0.2f, 0.4f, 0.65f, 1.0f };
	char buf[80];
	for (float rel : gridRel) {
		const float gx = mapX(rel * safeLen);
		dl->AddLine(ImVec2(gx, tl.y), ImVec2(gx, rb.y), gridColor, 1.0f);
		std::snprintf(buf, sizeof(buf), "%.2fL", rel);
		const ImVec2 ts = ImGui::CalcTextSize(buf);
		dl->AddText(ImVec2(gx - ts.x * 0.5f, rb.y + 4.0f), textColor, buf);
	}

	const float zeroY = mapY(0.0f);
	dl->AddLine(ImVec2(tl.x, tl.y), ImVec2(tl.x, rb.y), axisColor, 1.0f);
	dl->AddLine(ImVec2(tl.x, zeroY), ImVec2(rb.x, zeroY), axisColor, 1.5f);

	dl->AddText(ImVec2(origin.x + 4, tl.y), textColor, yMaxBuf);
	dl->AddText(ImVec2(origin.x + 4, rb.y - textH), textColor, yMinBuf);
	if (std::abs(zeroY - tl.y) > textH && std::abs(zeroY - rb.y) > textH) {
		dl->AddText(ImVec2(origin.x + 4, zeroY - textH * 0.5f), textColor, "0");
	}

	for (const PlotSeries& s : series) {
		const size_t n = std::min(x.size(), s.values.size());
		if (n < 2) {
			continue;
		}
		for (size_t i = 0; i + 1 < n; ++i) {
			const ImVec2 p0(mapX(x[i]),     mapY(s.values[i]));
			const ImVec2 p1(mapX(x[i + 1]), mapY(s.values[i + 1]));
			const ImVec2 z0(mapX(x[i]),     zeroY);
			const ImVec2 z1(mapX(x[i + 1]), zeroY);
			dl->AddQuadFilled(p0, p1, z1, z0, s.fill);
		}
		std::vector<ImVec2> pts;
		pts.reserve(n);
		for (size_t i = 0; i < n; ++i) {
			pts.push_back(ImVec2(mapX(x[i]), mapY(s.values[i])));
		}
		dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), s.color, 0, 2.2f);
	}

	float legendY = tl.y + 4.0f;
	for (const PlotSeries& s : series) {
		const float lx = tl.x + 10.0f;
		dl->AddRectFilled(ImVec2(lx, legendY), ImVec2(lx + 14.0f, legendY + 12.0f), s.color);
		dl->AddText(ImVec2(lx + 20.0f, legendY - 1.0f), IM_COL32(220, 220, 230, 255), s.name);
		legendY += 16.0f;
	}

	if (hovered && !x.empty()) {
		const float mouseX = std::clamp(ImGui::GetIO().MousePos.x, tl.x, rb.x);
		const float xVal = safeLen * (mouseX - tl.x) / (rb.x - tl.x);
		dl->AddLine(ImVec2(mouseX, tl.y), ImVec2(mouseX, rb.y), IM_COL32(230, 230, 240, 160), 1.0f);

		ImGui::BeginTooltip();
		ImGui::Text("x = %.1f м  (x/L = %.3f)", xVal, xVal / safeLen);
		for (const PlotSeries& s : series) {
			ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(s.color),
				"%s: %.1f", s.name, interpolateAt(x, s.values, xVal));
		}
		ImGui::EndTooltip();
	}
}

} // namespace

int main() {
	glfwSetErrorCallback(glfwErrorCallback);
	if (!glfwInit()) {
		std::fprintf(stderr, "Не удалось инициализировать GLFW\n");
		return 1;
	}

	const char* glslVersion = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

	GLFWwindow* window = glfwCreateWindow(
		1100, 860, "РМРС: прочность корпуса (часть II, п. 1.4)", nullptr, nullptr);
	if (window == nullptr) {
		std::fprintf(stderr, "Не удалось создать окно GLFW\n");
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // вертикальная синхронизация

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr; // не сохранять раскладку окон на диск
	loadCyrillicFont(io);

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glslVersion);

	// --- Состояние ввода ----------------------------------------------------
	rmrs::ShipParameters params;
	int areaIndex = 0;
	int typeIndex = 0;
	int sectionCount = 60;

	const rmrs::NavigationArea areas[] = {
		rmrs::NavigationArea::Unlimited, rmrs::NavigationArea::R0,
		rmrs::NavigationArea::R1, rmrs::NavigationArea::R2,
		rmrs::NavigationArea::R2_RSN, rmrs::NavigationArea::R2_RSN_4_5,
		rmrs::NavigationArea::R3_RSN, rmrs::NavigationArea::R3,
	};
	const char* areaLabels[] = {
		"Неограниченный", "R0", "R1", "R2", "R2-RSN", "R2-RSN(4,5)", "R3-RSN", "R3",
	};
	const rmrs::ShipType types[] = {
		rmrs::ShipType::General, rmrs::ShipType::ContainerShip,
		rmrs::ShipType::WideDeckOpening, rmrs::ShipType::TechnicalFleet,
		rmrs::ShipType::HighTemperatureCargo, rmrs::ShipType::Unusual,
	};
	const char* typeLabels[] = {
		"Обычное судно", "Контейнеровоз", "С широким раскрытием палубы",
		"Технический флот", "Груз при высокой температуре",
		"Необычная конструкция/назначение",
	};

	const ImVec4 kGreen (0.35f, 0.85f, 0.40f, 1.0f);
	const ImVec4 kYellow(0.95f, 0.80f, 0.25f, 1.0f);
	const ImVec4 kRed   (0.95f, 0.40f, 0.35f, 1.0f);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Окно ImGui на весь размер окна GLFW
		const ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(vp->WorkPos);
		ImGui::SetNextWindowSize(vp->WorkSize);
		ImGui::Begin("main", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoBringToFrontOnFocus);

		// --- Ввод параметров судна ------------------------------------------
		sectionHeader("Параметры судна");
		ImGui::PushItemWidth(220.0f);
		ImGui::InputDouble("Длина L, м", &params.length, 1.0, 10.0, "%.2f");
		ImGui::InputDouble("Ширина B, м", &params.breadth, 0.5, 5.0, "%.2f");
		ImGui::InputDouble("Высота борта D, м", &params.depth, 0.5, 5.0, "%.2f");
		ImGui::InputDouble("Коэф. общей полноты Cb", &params.blockCoeff, 0.01, 0.05, "%.3f");
		ImGui::InputDouble("Скорость v0, уз", &params.speed, 0.5, 2.0, "%.2f");

		if (ImGui::Combo("Район плавания", &areaIndex, areaLabels, IM_ARRAYSIZE(areaLabels))) {
			params.area = areas[areaIndex];
		}
		params.area = areas[areaIndex];
		if (ImGui::Combo("Тип судна", &typeIndex, typeLabels, IM_ARRAYSIZE(typeLabels))) {
			params.type = types[typeIndex];
		}
		params.type = types[typeIndex];
		ImGui::Checkbox("Стальное сварное судно", &params.steelWelded);
		ImGui::PopItemWidth();

		const double vLimit = rmrs::ApplicabilityChecker::speedThreshold(params.length);
		ImGui::TextDisabled(
			"L/B = %.2f   L/D = %.2f   B/D = %.2f   v = k*sqrt(L) = %.2f уз",
			params.lengthToBreadth(), params.lengthToDepth(),
			params.breadthToDepth(), vLimit);

		// --- Проверка применимости (п. 1.4.1.1 и 1.4.1.2) -------------------
		const rmrs::ApplicabilityResult res = rmrs::ApplicabilityChecker::evaluate(params);

		sectionHeader("Вывод по области распространения (п. 1.4.1.1, 1.4.1.2)");
		switch (res.verdict) {
			case rmrs::Verdict::NotInScope:
				ImGui::TextColored(kRed,
					"Судно НЕ подпадает под требования главы 1.4 (п. 1.4.1.1)");
				break;
			case rmrs::Verdict::DirectCalculationRequired:
				ImGui::TextColored(kYellow,
					"Судно подпадает под главу 1.4, но требуется ПРЯМОЙ расчёт "
					"прочности (п. 1.4.1.2)");
				break;
			case rmrs::Verdict::FormulaApplicable:
				ImGui::TextColored(kGreen,
					"Судно подпадает под главу 1.4; применимы формулы п. 1.4.4");
				break;
		}
		if (!res.outOfScopeReasons.empty()) {
			ImGui::TextColored(kRed, "Вне области распространения:");
			renderReasons(res.outOfScopeReasons);
		}
		if (!res.directCalcReasons.empty()) {
			ImGui::TextColored(kYellow, "Основания для прямого расчёта:");
			renderReasons(res.directCalcReasons);
		}
		if (!res.additionalNotes.empty()) {
			ImGui::Text("Дополнительно по п. 1.4.1.1:");
			renderReasons(res.additionalNotes);
		}

		// --- Расчёт волновых нагрузок (п. 1.4.4) ----------------------------
		sectionHeader("Волновые нагрузки (п. 1.4.4)");
		if (params.length <= 0.0 || params.breadth <= 0.0) {
			ImGui::TextColored(kRed, "Введите положительные L и B");
		} else {
			const rmrs::WaveLoadCalculator calc(
				params.length, params.breadth, params.blockCoeff, params.area);

			ImGui::Text("Волновой коэффициент cw = %.3f", calc.waveCoefficient());
			ImGui::SameLine();
			ImGui::Text("|  Cb (расчётный, >= 0,6) = %.3f", calc.effectiveBlock());
			ImGui::SameLine();
			ImGui::Text("|  Редукц. коэф. phi = %.3f", calc.reductionFactor());
			ImGui::TextDisabled(
				"psi = 1, v = 1 (упрощение п. 1.4.4.3: нет геометрии носа); "
				"phi по табл. 1.4.4.3 только для ограниченных районов при 60...150 м");

			if (res.verdict == rmrs::Verdict::DirectCalculationRequired) {
				ImGui::TextColored(kYellow,
					"Внимание: значения ниже приведены справочно; по п. 1.4.1.2 "
					"требуется прямой расчёт");
			} else if (res.verdict == rmrs::Verdict::NotInScope) {
				ImGui::TextColored(kRed,
					"Внимание: судно вне области распространения; значения носят "
					"исключительно демонстрационный характер");
			}

			ImGui::SliderInt("Число участков по длине", &sectionCount, 20, 120);

			const std::vector<rmrs::SectionLoads> rows = calc.distribution(sectionCount);

			std::vector<float> xs;
			std::vector<float> hog;
			std::vector<float> sag;
			std::vector<float> shearPos;
			std::vector<float> shearNeg;
			xs.reserve(rows.size());
			hog.reserve(rows.size());
			sag.reserve(rows.size());
			shearPos.reserve(rows.size());
			shearNeg.reserve(rows.size());

			float maxHog = 0.0f;
			float minSag = 0.0f;
			float maxShear = 0.0f;
			float minShear = 0.0f;
			for (const rmrs::SectionLoads& s : rows) {
				xs.push_back(static_cast<float>(s.x));
				hog.push_back(static_cast<float>(s.momentHog));
				sag.push_back(static_cast<float>(s.momentSag));
				shearPos.push_back(static_cast<float>(s.shearPos));
				shearNeg.push_back(static_cast<float>(s.shearNeg));
				maxHog = std::max(maxHog, static_cast<float>(s.momentHog));
				minSag = std::min(minSag, static_cast<float>(s.momentSag));
				maxShear = std::max(maxShear, static_cast<float>(s.shearPos));
				minShear = std::min(minShear, static_cast<float>(s.shearNeg));
			}

			const float L = static_cast<float>(params.length);
			const ImU32 hogColor = IM_COL32(80, 180, 255, 255);
			const ImU32 sagColor = IM_COL32(255, 140, 90, 255);
			const ImU32 posColor = IM_COL32(120, 220, 140, 255);
			const ImU32 negColor = IM_COL32(230, 110, 130, 255);

			drawEpure("Эпюра волнового изгибающего момента Mw, кН*м",
				xs,
				{
					{ "перегиб (hogging)", hogColor, withAlpha(hogColor, 70), hog },
					{ "прогиб (sagging)",  sagColor, withAlpha(sagColor, 70), sag },
				},
				L);

			ImGui::Text("Максимум перегиба: %.1f кН*м   |   Максимум прогиба: %.1f кН*м",
				maxHog, minSag);

			drawEpure("Эпюра волновой перерезывающей силы Nw, кН",
				xs,
				{
					{ "Nw (+)", posColor, withAlpha(posColor, 70), shearPos },
					{ "Nw (-)", negColor, withAlpha(negColor, 70), shearNeg },
				},
				L);

			ImGui::Text("Максимум Nw(+): %.1f кН   |   Максимум Nw(-): %.1f кН",
				maxShear, minShear);
			ImGui::TextDisabled("Наведи курсор на эпюру, покажу значения в сечении");
		}

		ImGui::End();

		ImGui::Render();
		int w = 0;
		int h = 0;
		glfwGetFramebufferSize(window, &w, &h);
		glViewport(0, 0, w, h);
		glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
