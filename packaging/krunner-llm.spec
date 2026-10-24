Name:           krunner-llm
Version:        0.1.0
Release:        1%{?dist}
Summary:        KRunner plugin that answers "ai <query>" via an OpenAI-compatible LLM API
License:        MIT
URL:            https://github.com/tristianc/krunner-llm
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  kf6-rpm-macros
BuildRequires:  qt6-qtbase-devel
BuildRequires:  kf6-krunner-devel
BuildRequires:  kf6-kconfig-devel
BuildRequires:  kf6-ki18n-devel
BuildRequires:  kf6-kcoreaddons-devel
BuildRequires:  kf6-kcmutils-devel
BuildRequires:  kf6-kconfigwidgets-devel

Requires:       kf6-krunner%{?_isa}
Requires:       kf6-kcmutils%{?_isa}

%description
A KRunner plugin for KDE Plasma 6. Type "ai <question>" in KRunner and the
answer comes back from any OpenAI-compatible chat completions endpoint
(OpenRouter by default). Includes a settings module for the API key, URL,
model and debounce delay.

%prep
%autosetup

%build
%cmake_kf6
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%doc README.md
%{_kf6_qtplugindir}/kf6/krunner/krunner_llm.so
%{_kf6_qtplugindir}/kf6/krunner/kcms/kcm_krunner_llm.so

%changelog
* Sat Oct 24 2026 Tristian Celestin <tristin.celestin@gmail.com> - 0.1.0-1
- Initial package
