#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QSlider>
#include <QTimer>
#include <QWidget>

#include "engine.hpp"

// On/off checkbox + brightness slider + color temperature slider.
// Slider moves are coalesced for a short delay so a drag sends one request, not hundreds.
class PatchControls : public QWidget {
	Q_OBJECT
public:
	explicit PatchControls(QWidget *parent = nullptr);
	void setState(const LightState &st, bool reachable);

signals:
	void patch(const LightPatch &p);

private:
	void queue(const LightPatch &p);
	void flush();

	QCheckBox *on_;
	QSlider *brightness_;
	QSlider *kelvin_;
	QLabel *brightnessLabel_;
	QLabel *kelvinLabel_;
	QLabel *status_;
	QTimer timer_;
	LightPatch pending_;
};
