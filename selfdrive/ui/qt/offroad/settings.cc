#include <cassert>
#include <cmath>
#include <string>
#include <tuple>
#include <vector>

#include <QDebug>

// Add these
#include <algorithm>      // For std::max, std::min
#include <QLineEdit>      // For weight input field
#include <QPushButton>    // For +/- buttons (already in settings.h but good practice)
#include <QLabel>         // For unit label
#include <QHBoxLayout>    // For horizontal layout

#include "common/watchdog.h"
#include "common/util.h"
#include "selfdrive/ui/qt/network/networking.h"
#include "selfdrive/ui/qt/offroad/settings.h"
#include "selfdrive/ui/qt/qt_window.h"
#include "selfdrive/ui/qt/widgets/prime.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"
#include "selfdrive/ui/qt/offroad/developer_panel.h"
#include "selfdrive/ui/qt/offroad/firehose.h"

// new weight parameter file
#include "selfdrive/ui/qt/weight_config.h"

TogglesPanel::TogglesPanel(SettingsWindow *parent) : ListWidget(parent) {
  // param, title, desc, icon, restart needed
  std::vector<std::tuple<QString, QString, QString, QString, bool>> toggle_defs{
    {
      "OpenpilotEnabledToggle",
      tr("Enable openpilot"),
      tr("Use the openpilot system for adaptive cruise control and lane keep driver assistance. Your attention is required at all times to use this feature."),
      "../assets/icons/chffr_wheel.png",
      true,
    },
    {
      "ExperimentalMode",
      tr("Experimental Mode"),
      "",
      "../assets/icons/experimental_white.svg",
      false,
    },
    {
      "DisengageOnAccelerator",
      tr("Disengage on Accelerator Pedal"),
      tr("When enabled, pressing the accelerator pedal will disengage openpilot."),
      "../assets/icons/disengage_on_accelerator.svg",
      false,
    },
    {
      "IsLdwEnabled",
      tr("Enable Lane Departure Warnings"),
      tr("Receive alerts to steer back into the lane when your vehicle drifts over a detected lane line without a turn signal activated while driving over 31 mph (50 km/h)."),
      "../assets/icons/warning.png",
      false,
    },
    {
      "AlwaysOnDM",
      tr("Always-On Driver Monitoring"),
      tr("Enable driver monitoring even when openpilot is not engaged."),
      "../assets/icons/monitoring.png",
      false,
    },
    {
      "RecordFront",
      tr("Record and Upload Driver Camera"),
      tr("Upload data from the driver facing camera and help improve the driver monitoring algorithm."),
      "../assets/icons/monitoring.png",
      true,
    },
    {
      "IsMetric",
      tr("Use Metric System"),
      tr("Display speed in km/h instead of mph."),
      "../assets/icons/metric.png",
      false,
    },
  };


  std::vector<QString> longi_button_texts{tr("Aggressive"), tr("Standard"), tr("Relaxed")};
  long_personality_setting = new ButtonParamControl("LongitudinalPersonality", tr("Driving Personality"),
                                          tr("Standard is recommended. In aggressive mode, openpilot will follow lead cars closer and be more aggressive with the gas and brake. "
                                             "In relaxed mode openpilot will stay further away from lead cars. On supported cars, you can cycle through these personalities with "
                                             "your steering wheel distance button."),
                                          "../assets/icons/speed_limit.png",
                                          longi_button_texts);

  // set up uiState update for personality setting
  QObject::connect(uiState(), &UIState::uiUpdate, this, &TogglesPanel::updateState);

  for (auto &[param, title, desc, icon, needs_restart] : toggle_defs) {
    auto toggle = new ParamControl(param, title, desc, icon, this);

    bool locked = params.getBool((param + "Lock").toStdString());
    toggle->setEnabled(!locked);

    if (needs_restart && !locked) {
      toggle->setDescription(toggle->getDescription() + tr(" Changing this setting will restart openpilot if the car is powered on."));

      QObject::connect(uiState(), &UIState::engagedChanged, [toggle](bool engaged) {
        toggle->setEnabled(!engaged);
      });

      QObject::connect(toggle, &ParamControl::toggleFlipped, [=](bool state) {
        params.putBool("OnroadCycleRequested", true);
      });
    }

    addItem(toggle);
    toggles[param.toStdString()] = toggle;

    // insert longitudinal personality after NDOG toggle
    if (param == "DisengageOnAccelerator") {
      addItem(long_personality_setting);
    }
  }

  // ========== ADD VEHICLE WEIGHT INPUT HERE ==========

  // Vehicle Weight Input
  /*
  auto vehicle_weight_widget = new AbstractControl(
    tr("Vehicle Weight"),
    tr("Enter total vehicle weight including cargo. Range: 500-5000 kg. Used for longitudinal control optimization."),
    "../assets/offroad/icon_road.png",
    this
  ); */
  // Create widget container
  QWidget *vehicle_weight_widget = new QWidget(this);
  vehicle_weight_widget->setObjectName("vehicle_weight_widget");

  // Create main vertical layout
  QVBoxLayout *vbox = new QVBoxLayout(vehicle_weight_widget);
  vbox->setContentsMargins(50, 20, 50, 20);

  // Title
  QLabel *title = new QLabel(tr("Vehicle Weight"), vehicle_weight_widget);
  title->setStyleSheet("font-size: 50px; font-weight: 600;");

  // Description
  QLabel *desc = new QLabel(tr("Trailer weight. Set to 0 when not towing. Range: %1-%2 kg.")
  .arg(WEIGHT_MIN).arg(WEIGHT_MAX),  // Dynamic range
  vehicle_weight_widget);
  desc->setStyleSheet("font-size: 40px; color: #A0A0A0;");
  desc->setWordWrap(true);

  // Add title and description to layout
  vbox->addWidget(title);
  vbox->addWidget(desc);
  vbox->addSpacing(20);

   // Create horizontal layout for controls
  QHBoxLayout *weight_layout = new QHBoxLayout();
  weight_layout->setSpacing(10);

  weight_minus_btn = new QPushButton(QString("-%1").arg(WEIGHT_STEP));  // Was "-50"
  weight_minus_btn->setFixedSize(80, 60);
  weight_minus_btn->setStyleSheet(
    "QPushButton { font-size: 28px; font-weight: bold; "
    "background-color: #E74C3C; color: white; border-radius: 8px; }"
    "QPushButton:pressed { background-color: #C0392B; }"
  );
  // Minus button
  QLineEdit *weight_input = new QLineEdit();
  weight_input->setPlaceholderText(QString::number(WEIGHT_DEFAULT));
  weight_input->setFixedWidth(120);
  weight_input->setAlignment(Qt::AlignCenter);
  weight_input->setStyleSheet(
    "font-size: 36px; padding: 10px; "
    "border: 2px solid #3498DB; border-radius: 8px;"
  );
   // Load current value from parameter
  // QString current_weight = QString::fromStdString(params.get("VehicleWeight"));
  QString current_weight = QString::fromStdString(params.get("TrailerWeight"));
  if (!current_weight.isEmpty()) {
    weight_input->setText(current_weight);
  } else {
    weight_input->setText(QString::number(WEIGHT_DEFAULT));  // Was "1500"
  }

  weight_plus_btn = new QPushButton(QString("+%1").arg(WEIGHT_STEP));   // Was "+50"
  weight_plus_btn->setFixedSize(80, 60);
  weight_plus_btn->setStyleSheet(
    "QPushButton { font-size: 28px; font-weight: bold; "
    "background-color: #27AE60; color: white; border-radius: 8px; }"
    "QPushButton:pressed { background-color: #229954; }"
  );

  QLabel *unit_label = new QLabel("kg");
  unit_label->setStyleSheet("font-size: 28px; color: #7F8C8D;");

  QObject::connect(weight_minus_btn, &QPushButton::clicked, [=]() {
    int current = weight_input->text().toInt();
    int new_weight = std::max(WEIGHT_MIN, current - WEIGHT_STEP);  // Was 500, 50
    weight_input->setText(QString::number(new_weight));
    // params.put("VehicleWeight", std::to_string(new_weight));
    params.put("TrailerWeight", std::to_string(new_weight));
  });

  QObject::connect(weight_plus_btn, &QPushButton::clicked, [=]() {
    int current = weight_input->text().toInt();
    int new_weight = std::min(WEIGHT_MAX, current + WEIGHT_STEP);  // Was 5000, 50
    weight_input->setText(QString::number(new_weight));
    // params.put("VehicleWeight", std::to_string(new_weight));
    params.put("TrailerWeight", std::to_string(new_weight));
  });

  QObject::connect(weight_input, &QLineEdit::editingFinished, [=]() {
    bool ok;
    int weight = weight_input->text().toInt(&ok);
    if (ok && weight >= WEIGHT_MIN && weight <= WEIGHT_MAX) {  // Was 500, 5000
      // params.put("VehicleWeight", std::to_string(weight));
      params.put("TrailerWeight", std::to_string(weight));
      weight_input->setStyleSheet(
        "font-size: 36px; padding: 10px; "
        "border: 2px solid #27AE60; border-radius: 8px;"
      );
    } else {
      weight_input->setStyleSheet(
        "font-size: 36px; padding: 10px; "
        "border: 2px solid #E74C3C; border-radius: 8px;"
      );
    }
  });

  weight_layout->addWidget(weight_minus_btn);
  weight_layout->addWidget(weight_input);
  weight_layout->addWidget(unit_label);
  weight_layout->addWidget(weight_plus_btn);
  weight_layout->addStretch();

  vbox->addLayout(weight_layout);

  QFrame *line = new QFrame(vehicle_weight_widget);
  line->setFrameShape(QFrame::HLine);
  line->setStyleSheet("color: #404040;");
  vbox->addWidget(line);

  addItem(vehicle_weight_widget);

  // Toggles with confirmation dialogs

  // ========== END VEHICLE WEIGHT INPUT ==========

  // Toggles with confirmation dialogs (line 112-114 in original)
  toggles["ExperimentalMode"]->setActiveIcon("../assets/icons/experimental.svg");
  toggles["ExperimentalMode"]->setConfirmation(true, true);
}  // End of constructor

void TogglesPanel::updateState(const UIState &s) {
  const SubMaster &sm = *(s.sm);

  if (sm.updated("selfdriveState")) {
    auto personality = sm["selfdriveState"].getSelfdriveState().getPersonality();
    if (personality != s.scene.personality && s.scene.started && isVisible()) {
      long_personality_setting->setCheckedButton(static_cast<int>(personality));
    }
    uiState()->scene.personality = personality;
  }
}

void TogglesPanel::expandToggleDescription(const QString &param) {
  toggles[param.toStdString()]->showDescription();
}

void TogglesPanel::showEvent(QShowEvent *event) {
  updateToggles();
}

void TogglesPanel::updateToggles() {
  auto experimental_mode_toggle = toggles["ExperimentalMode"];
  const QString e2e_description = QString("%1<br>"
                                          "<h4>%2</h4><br>"
                                          "%3<br>"
                                          "<h4>%4</h4><br>"
                                          "%5<br>")
                                  .arg(tr("openpilot defaults to driving in <b>chill mode</b>. Experimental mode enables <b>alpha-level features</b> that aren't ready for chill mode. Experimental features are listed below:"))
                                  .arg(tr("End-to-End Longitudinal Control"))
                                  .arg(tr("Let the driving model control the gas and brakes. openpilot will drive as it thinks a human would, including stopping for red lights and stop signs. "
                                          "Since the driving model decides the speed to drive, the set speed will only act as an upper bound. This is an alpha quality feature; "
                                          "mistakes should be expected."))
                                  .arg(tr("New Driving Visualization"))
                                  .arg(tr("The driving visualization will transition to the road-facing wide-angle camera at low speeds to better show some turns. The Experimental mode logo will also be shown in the top right corner."));

  const bool is_release = params.getBool("IsReleaseBranch");
  auto cp_bytes = params.get("CarParamsPersistent");
  if (!cp_bytes.empty()) {
    AlignedBuffer aligned_buf;
    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(cp_bytes.data(), cp_bytes.size()));
    cereal::CarParams::Reader CP = cmsg.getRoot<cereal::CarParams>();

    if (hasLongitudinalControl(CP)) {
      // normal description and toggle
      experimental_mode_toggle->setEnabled(true);
      experimental_mode_toggle->setDescription(e2e_description);
      long_personality_setting->setEnabled(true);
    } else {
      // no long for now
      experimental_mode_toggle->setEnabled(false);
      long_personality_setting->setEnabled(false);
      params.remove("ExperimentalMode");

      const QString unavailable = tr("Experimental mode is currently unavailable on this car since the car's stock ACC is used for longitudinal control.");

      QString long_desc = unavailable + " " + \
                          tr("openpilot longitudinal control may come in a future update.");
      if (CP.getAlphaLongitudinalAvailable()) {
        if (is_release) {
          long_desc = unavailable + " " + tr("An alpha version of openpilot longitudinal control can be tested, along with Experimental mode, on non-release branches.");
        } else {
          long_desc = tr("Enable the openpilot longitudinal control (alpha) toggle to allow Experimental mode.");
        }
      }
      experimental_mode_toggle->setDescription("<b>" + long_desc + "</b><br><br>" + e2e_description);
    }

    experimental_mode_toggle->refresh();
  } else {
    experimental_mode_toggle->setDescription(e2e_description);
  }
}

DevicePanel::DevicePanel(SettingsWindow *parent) : ListWidget(parent) {
  setSpacing(50);
  addItem(new LabelControl(tr("Dongle ID"), getDongleId().value_or(tr("N/A"))));
  addItem(new LabelControl(tr("Serial"), params.get("HardwareSerial").c_str()));

  pair_device = new ButtonControl(tr("Pair Device"), tr("PAIR"),
                                  tr("Pair your device with comma connect (connect.comma.ai) and claim your comma prime offer."));
  connect(pair_device, &ButtonControl::clicked, [=]() {
    PairingPopup popup(this);
    popup.exec();
  });
  addItem(pair_device);

  // offroad-only buttons

  auto dcamBtn = new ButtonControl(tr("Driver Camera"), tr("PREVIEW"),
                                   tr("Preview the driver facing camera to ensure that driver monitoring has good visibility. (vehicle must be off)"));
  connect(dcamBtn, &ButtonControl::clicked, [=]() { emit showDriverView(); });
  addItem(dcamBtn);

  auto resetCalibBtn = new ButtonControl(tr("Reset Calibration"), tr("RESET"), "");
  connect(resetCalibBtn, &ButtonControl::showDescriptionEvent, this, &DevicePanel::updateCalibDescription);
  connect(resetCalibBtn, &ButtonControl::clicked, [&]() {
    if (!uiState()->engaged()) {
      if (ConfirmationDialog::confirm(tr("Are you sure you want to reset calibration?"), tr("Reset"), this)) {
        // Check engaged again in case it changed while the dialog was open
        if (!uiState()->engaged()) {
          params.remove("CalibrationParams");
          params.remove("LiveTorqueParameters");
          params.remove("LiveParameters");
          params.remove("LiveParametersV2");
          params.remove("LiveDelay");
          params.putBool("OnroadCycleRequested", true);
        }
      }
    } else {
      ConfirmationDialog::alert(tr("Disengage to Reset Calibration"), this);
    }
  });
  addItem(resetCalibBtn);

  auto retrainingBtn = new ButtonControl(tr("Review Training Guide"), tr("REVIEW"), tr("Review the rules, features, and limitations of openpilot"));
  connect(retrainingBtn, &ButtonControl::clicked, [=]() {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to review the training guide?"), tr("Review"), this)) {
      emit reviewTrainingGuide();
    }
  });
  addItem(retrainingBtn);

  if (Hardware::TICI()) {
    auto regulatoryBtn = new ButtonControl(tr("Regulatory"), tr("VIEW"), "");
    connect(regulatoryBtn, &ButtonControl::clicked, [=]() {
      const std::string txt = util::read_file("../assets/offroad/fcc.html");
      ConfirmationDialog::rich(QString::fromStdString(txt), this);
    });
    addItem(regulatoryBtn);
  }

  auto translateBtn = new ButtonControl(tr("Change Language"), tr("CHANGE"), "");
  connect(translateBtn, &ButtonControl::clicked, [=]() {
    QMap<QString, QString> langs = getSupportedLanguages();
    QString selection = MultiOptionDialog::getSelection(tr("Select a language"), langs.keys(), langs.key(uiState()->language), this);
    if (!selection.isEmpty()) {
      // put language setting, exit Qt UI, and trigger fast restart
      params.put("LanguageSetting", langs[selection].toStdString());
      qApp->exit(18);
      watchdog_kick(0);
    }
  });
  addItem(translateBtn);

  QObject::connect(uiState()->prime_state, &PrimeState::changed, [this] (PrimeState::Type type) {
    pair_device->setVisible(type == PrimeState::PRIME_TYPE_UNPAIRED);
  });
  QObject::connect(uiState(), &UIState::offroadTransition, [=](bool offroad) {
    for (auto btn : findChildren<ButtonControl *>()) {
      if (btn != pair_device && btn != resetCalibBtn) {
        btn->setEnabled(offroad);
      }
    }
  });

  // power buttons
  QHBoxLayout *power_layout = new QHBoxLayout();
  power_layout->setSpacing(30);

  QPushButton *reboot_btn = new QPushButton(tr("Reboot"));
  reboot_btn->setObjectName("reboot_btn");
  power_layout->addWidget(reboot_btn);
  QObject::connect(reboot_btn, &QPushButton::clicked, this, &DevicePanel::reboot);

  QPushButton *poweroff_btn = new QPushButton(tr("Power Off"));
  poweroff_btn->setObjectName("poweroff_btn");
  power_layout->addWidget(poweroff_btn);
  QObject::connect(poweroff_btn, &QPushButton::clicked, this, &DevicePanel::poweroff);

  if (!Hardware::PC()) {
    connect(uiState(), &UIState::offroadTransition, poweroff_btn, &QPushButton::setVisible);
  }

  setStyleSheet(R"(
    #reboot_btn { height: 120px; border-radius: 15px; background-color: #393939; }
    #reboot_btn:pressed { background-color: #4a4a4a; }
    #poweroff_btn { height: 120px; border-radius: 15px; background-color: #E22C2C; }
    #poweroff_btn:pressed { background-color: #FF2424; }
  )");
  addItem(power_layout);
}

void DevicePanel::updateCalibDescription() {
  QString desc =
      tr("openpilot requires the device to be mounted within 4° left or right and "
         "within 5° up or 9° down. openpilot is continuously calibrating, resetting is rarely required.");
  std::string calib_bytes = params.get("CalibrationParams");
  if (!calib_bytes.empty()) {
    try {
      AlignedBuffer aligned_buf;
      capnp::FlatArrayMessageReader cmsg(aligned_buf.align(calib_bytes.data(), calib_bytes.size()));
      auto calib = cmsg.getRoot<cereal::Event>().getLiveCalibration();
      if (calib.getCalStatus() != cereal::LiveCalibrationData::Status::UNCALIBRATED) {
        double pitch = calib.getRpyCalib()[1] * (180 / M_PI);
        double yaw = calib.getRpyCalib()[2] * (180 / M_PI);
        desc += tr(" Your device is pointed %1° %2 and %3° %4.")
                    .arg(QString::number(std::abs(pitch), 'g', 1), pitch > 0 ? tr("down") : tr("up"),
                         QString::number(std::abs(yaw), 'g', 1), yaw > 0 ? tr("left") : tr("right"));
      }
    } catch (kj::Exception) {
      qInfo() << "invalid CalibrationParams";
    }
  }
  desc += tr(" Resetting calibration will restart openpilot if the car is powered on.");
  qobject_cast<ButtonControl *>(sender())->setDescription(desc);
}

void DevicePanel::reboot() {
  if (!uiState()->engaged()) {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to reboot?"), tr("Reboot"), this)) {
      // Check engaged again in case it changed while the dialog was open
      if (!uiState()->engaged()) {
        params.putBool("DoReboot", true);
      }
    }
  } else {
    ConfirmationDialog::alert(tr("Disengage to Reboot"), this);
  }
}

void DevicePanel::poweroff() {
  if (!uiState()->engaged()) {
    if (ConfirmationDialog::confirm(tr("Are you sure you want to power off?"), tr("Power Off"), this)) {
      // Check engaged again in case it changed while the dialog was open
      if (!uiState()->engaged()) {
        params.putBool("DoShutdown", true);
      }
    }
  } else {
    ConfirmationDialog::alert(tr("Disengage to Power Off"), this);
  }
}

void SettingsWindow::showEvent(QShowEvent *event) {
  setCurrentPanel(0);
}

void SettingsWindow::setCurrentPanel(int index, const QString &param) {
  if (!param.isEmpty()) {
    // Check if param ends with "Panel" to determine if it's a panel name
    if (param.endsWith("Panel")) {
      QString panelName = param;
      panelName.chop(5); // Remove "Panel" suffix

      // Find the panel by name
      for (int i = 0; i < nav_btns->buttons().size(); i++) {
        if (nav_btns->buttons()[i]->text() == tr(panelName.toStdString().c_str())) {
          index = i;
          break;
        }
      }
    } else {
      emit expandToggleDescription(param);
    }
  }

  panel_widget->setCurrentIndex(index);
  nav_btns->buttons()[index]->setChecked(true);
}

SettingsWindow::SettingsWindow(QWidget *parent) : QFrame(parent) {

  // setup two main layouts
  sidebar_widget = new QWidget;
  QVBoxLayout *sidebar_layout = new QVBoxLayout(sidebar_widget);
  panel_widget = new QStackedWidget();

  // close button
  QPushButton *close_btn = new QPushButton(tr("×"));
  close_btn->setStyleSheet(R"(
    QPushButton {
      font-size: 140px;
      padding-bottom: 20px;
      border-radius: 100px;
      background-color: #292929;
      font-weight: 400;
    }
    QPushButton:pressed {
      background-color: #3B3B3B;
    }
  )");
  close_btn->setFixedSize(200, 200);
  sidebar_layout->addSpacing(45);
  sidebar_layout->addWidget(close_btn, 0, Qt::AlignCenter);
  QObject::connect(close_btn, &QPushButton::clicked, this, &SettingsWindow::closeSettings);

  // setup panels
  DevicePanel *device = new DevicePanel(this);
  QObject::connect(device, &DevicePanel::reviewTrainingGuide, this, &SettingsWindow::reviewTrainingGuide);
  QObject::connect(device, &DevicePanel::showDriverView, this, &SettingsWindow::showDriverView);

  TogglesPanel *toggles = new TogglesPanel(this);
  QObject::connect(this, &SettingsWindow::expandToggleDescription, toggles, &TogglesPanel::expandToggleDescription);

  auto networking = new Networking(this);
  QObject::connect(uiState()->prime_state, &PrimeState::changed, networking, &Networking::setPrimeType);

  QList<QPair<QString, QWidget *>> panels = {
    {tr("Device"), device},
    {tr("Network"), networking},
    {tr("Toggles"), toggles},
    {tr("Software"), new SoftwarePanel(this)},
    {tr("Firehose"), new FirehosePanel(this)},
    {tr("Developer"), new DeveloperPanel(this)},
  };

  nav_btns = new QButtonGroup(this);
  for (auto &[name, panel] : panels) {
    QPushButton *btn = new QPushButton(name);
    btn->setCheckable(true);
    btn->setChecked(nav_btns->buttons().size() == 0);
    btn->setStyleSheet(R"(
      QPushButton {
        color: grey;
        border: none;
        background: none;
        font-size: 65px;
        font-weight: 500;
      }
      QPushButton:checked {
        color: white;
      }
      QPushButton:pressed {
        color: #ADADAD;
      }
    )");
    btn->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    nav_btns->addButton(btn);
    sidebar_layout->addWidget(btn, 0, Qt::AlignRight);

    const int lr_margin = name != tr("Network") ? 50 : 0;  // Network panel handles its own margins
    panel->setContentsMargins(lr_margin, 25, lr_margin, 25);

    ScrollView *panel_frame = new ScrollView(panel, this);
    panel_widget->addWidget(panel_frame);

    QObject::connect(btn, &QPushButton::clicked, [=, w = panel_frame]() {
      btn->setChecked(true);
      panel_widget->setCurrentWidget(w);
    });
  }
  sidebar_layout->setContentsMargins(50, 50, 100, 50);

  // main settings layout, sidebar + main panel
  QHBoxLayout *main_layout = new QHBoxLayout(this);

  sidebar_widget->setFixedWidth(500);
  main_layout->addWidget(sidebar_widget);
  main_layout->addWidget(panel_widget);

  setStyleSheet(R"(
    * {
      color: white;
      font-size: 50px;
    }
    SettingsWindow {
      background-color: black;
    }
    QStackedWidget, ScrollView {
      background-color: #292929;
      border-radius: 30px;
    }
  )");
}
