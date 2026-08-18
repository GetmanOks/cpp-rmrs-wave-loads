#pragma once

#include <string>

namespace rmrs {

// Районы плавания судна по классификации РМРС (часть I "Классификация")
// От района зависят минимальная длина судна (п. 1.4.1.1)
// и редукционный коэффициент волновых нагрузок (табл. 1.4.4.3)
enum class NavigationArea {
	Unlimited,   // неограниченный район плавания
	R0,
	R1,
	R2,
	R2_RSN,
	R2_RSN_4_5,
	R3_RSN,
	R3
};

// Человекочитаемое имя района
inline std::string toString(NavigationArea area) {
	switch (area) {
		case NavigationArea::Unlimited:  return "Неограниченный";
		case NavigationArea::R0:         return "R0";
		case NavigationArea::R1:         return "R1";
		case NavigationArea::R2:         return "R2";
		case NavigationArea::R2_RSN:     return "R2-RSN";
		case NavigationArea::R2_RSN_4_5: return "R2-RSN(4,5)";
		case NavigationArea::R3_RSN:     return "R3-RSN";
		case NavigationArea::R3:         return "R3";
	}
	return "?";
}

// Районы с порогом L >= 65 м по п. 1.4.1.1: неограниченный, R0, R1, R2
// Для R2-RSN / R3-RSN / R3 порог 60 м
inline bool isSeaArea(NavigationArea area) {
	return area == NavigationArea::Unlimited
		|| area == NavigationArea::R0
		|| area == NavigationArea::R1
		|| area == NavigationArea::R2;
}

} // namespace rmrs
