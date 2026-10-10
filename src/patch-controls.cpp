#include "patch-controls.hpp"

#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "ui-util.hpp"

namespace {
constexpr int kCoalesceMs = 80;

QString percentText(int v)
{
	return QString("%1 %").arg(v);
}

QString kelvinText(int k)
{
	return QString("%1 K").arg(k);
}

QHBoxLayout *sliderRow(const QString &caption, QSlider *slider, QLabel *value)
{
	auto *row = new QHBoxLayout();
	row->addWidget(new QLabel(caption));
	row->addWidget(slider, 1);
	row->addWidget(value);
	return row;
}
} // namespace

PatchControls::PatchControls(QWidget *parent) : QWidget(parent)
{
	on_ = new QCheckBox(U("Allumée"));

	brightness_ = new QSlider(Qt::Horizontal);
	brightness_->setRange(1, 100);
	brightnessLabel_ = new QLabel(percentText(50));
	brightnessLabel_->setMinimumWidth(48);

	kelvin_ = new QSlider(Qt::Horizontal);
	kelvin_->setRange(miredToKelvin(kMaxMired), miredToKelvin(kMinMired));
	kelvinLabel_ = new QLabel(kelvinText(5000));
	kelvinLabel_->setMinimumWidth(48);

	status_ = new QLabel();

	auto *column = new QVBoxLayout(this);
	column->setContentsMargins(0, 0, 0, 0);
	auto *header = new QHBoxLayout();
	header->addWidget(on_);
	header->addStretch();
	header->addWidget(status_);
	column->addLayout(header);
	column->addLayout(sliderRow(U("Luminosité"), brightness_, brightnessLabel_));
	column->addLayout(sliderRow(U("Température"), kelvin_, kelvinLabel_));

	timer_.setSingleShot(true);
	timer_.setInterval(kCoalesceMs);
	connect(&timer_, &QTimer::timeout, this, &PatchControls::flush);

	connect(on_, &QCheckBox::toggled, this, [this](bool v) {
		LightPatch p;
		p.on = v;
		queue(p);
	});
	connect(brightness_, &QSlider::valueChanged, this, [this](int v) {
		brightnessLabel_->setText(percentText(v));
		LightPatch p;
		p.brightness = v;
		queue(p);
	});
	connect(kelvin_, &QSlider::valueChanged, this, [this](int v) {
		kelvinLabel_->setText(kelvinText(v));
		LightPatch p;
		p.temperature = kelvinToMired(v);
		queue(p);
	});
}

void PatchControls::queue(const LightPatch &p)
{
	if (p.on)
		pending_.on = p.on;
	if (p.brightness)
		pending_.brightness = p.brightness;
	if (p.temperature)
		pending_.temperature = p.temperature;
	timer_.start();
}

void PatchControls::flush()
{
	const LightPatch p = pending_;
	pending_ = LightPatch();
	emit patch(p);
}

void PatchControls::setState(const LightState &st, bool reachable)
{
	const QSignalBlocker b1(on_), b2(brightness_), b3(kelvin_);
	const int kelvin = miredToKelvin(st.temperature);
	on_->setChecked(st.on);
	brightness_->setValue(st.brightness);
	kelvin_->setValue(kelvin);
	brightnessLabel_->setText(percentText(st.brightness));
	kelvinLabel_->setText(kelvinText(kelvin));
	status_->setText(reachable ? QString() : U("⚠ injoignable"));
}
