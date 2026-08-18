#pragma once

#include "NavigationArea.h"
#include "ShipType.h"

namespace rmrs {

// Исходные данные о судне, вводимые пользователем
// Линейные величины в метрах, скорость в узлах
struct ShipParameters {
	double        length     = 100.0; // L  - длина судна, м
	double        breadth    = 16.0;  // B  - ширина судна, м
	double        depth      = 9.0;   // D  - высота борта, м
	double        blockCoeff = 0.70;  // Cb - коэффициент общей полноты
	double        speed      = 15.0;  // v0 - спецификационная скорость, уз
	NavigationArea area       = NavigationArea::Unlimited;
	ShipType       type       = ShipType::General;
	bool          steelWelded = true; // стальное судно сварной конструкции

	// Соотношения главных размерений
	double lengthToBreadth() const { return breadth  > 0.0 ? length / breadth : 0.0; }
	double lengthToDepth()   const { return depth    > 0.0 ? length / depth   : 0.0; }
	double breadthToDepth()  const { return depth    > 0.0 ? breadth / depth  : 0.0; }
};

} // namespace rmrs
