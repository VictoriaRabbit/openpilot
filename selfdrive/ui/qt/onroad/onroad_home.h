#pragma once

#include "selfdrive/ui/qt/onroad/alerts.h"
#include "selfdrive/ui/qt/onroad/annotated_camera.h"

#include <QPainter>
#include <QStackedLayout>   // ADD THIS LINE
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <algorithm>

#include "selfdrive/ui/qt/util.h"
#include "common/params.h"

class OnroadWindow : public QWidget {
  Q_OBJECT

public:
  OnroadWindow(QWidget* parent = 0);

private:
  void paintEvent(QPaintEvent *event);
  OnroadAlerts *alerts;
  AnnotatedCameraWidget *nvg;
  QColor bg = bg_colors[STATUS_DISENGAGED];
  QHBoxLayout* split;

  // ADD THESE THREE LINES:
  Params params;
  QPushButton *weight_minus_btn;
  QPushButton *weight_plus_btn;
  QLabel *weight_display;

private slots:
  void offroadTransition(bool offroad);
  void updateState(const UIState &s);
};
