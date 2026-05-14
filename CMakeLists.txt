cmake_minimum_required(VERSION 3.16)
project(krunner-llm)

set(KF6_MIN_VERSION "6.0.0")
set(QT_MIN_VERSION "6.5.0")

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Network Gui Widgets)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS Runner CoreAddons I18n Config KCMUtils ConfigWidgets)

kcoreaddons_add_plugin(krunner_llm SOURCES llmrunner.cpp INSTALL_NAMESPACE "kf6/krunner")

target_link_libraries(krunner_llm
    Qt6::Core
    Qt6::Network
    Qt6::Gui
    KF6::Runner
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::I18n
)

kcoreaddons_add_plugin(kcm_krunner_llm SOURCES kcm/kcm_krunner_llm.cpp INSTALL_NAMESPACE "kf6/krunner/kcms")

target_link_libraries(kcm_krunner_llm
    Qt6::Widgets
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::ConfigWidgets
    KF6::KCMUtils
    KF6::I18n
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
