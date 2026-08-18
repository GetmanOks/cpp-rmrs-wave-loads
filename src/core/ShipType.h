#pragma once

#include <string>

namespace rmrs {

// Тип судна: влияет на область распространения главы 1.4 и на то,
// требуется ли прямой расчёт прочности (п. 1.4.1.1 и 1.4.1.2)
enum class ShipType {
	General,               // обычное судно
	ContainerShip,         // контейнеровоз
	WideDeckOpening,       // судно с широким раскрытием палубы
	TechnicalFleet,        // судно технического флота
	HighTemperatureCargo,  // судно, перевозящее грузы при высокой температуре
	Unusual                // судно необычной конструкции и/или назначения
};

inline std::string toString(ShipType type) {
	switch (type) {
		case ShipType::General:              return "Обычное судно";
		case ShipType::ContainerShip:        return "Контейнеровоз";
		case ShipType::WideDeckOpening:      return "С широким раскрытием палубы";
		case ShipType::TechnicalFleet:       return "Технический флот";
		case ShipType::HighTemperatureCargo: return "Груз при высокой температуре";
		case ShipType::Unusual:              return "Необычная конструкция/назначение";
	}
	return "?";
}

} // namespace rmrs
