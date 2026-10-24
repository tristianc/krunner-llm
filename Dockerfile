# Builds krunner-llm as a Fedora 44 RPM.
#
#   podman build -t krunner-llm-rpm .
#   podman run --rm -v "$PWD/dist:/out:Z" krunner-llm-rpm
#   sudo dnf install ./dist/krunner-llm-*.x86_64.rpm
FROM registry.fedoraproject.org/fedora:44

RUN dnf -y install rpm-build rpmdevtools dnf-plugins-core && dnf clean all

# Install build deps straight from the spec so the two can't drift.
COPY packaging/krunner-llm.spec /tmp/krunner-llm.spec
RUN dnf -y builddep /tmp/krunner-llm.spec && dnf clean all

RUN rpmdev-setuptree
WORKDIR /src
COPY . /src/krunner-llm

# Tarball the source under the name/version the spec expects, then build.
RUN set -eu; \
    spec=/src/krunner-llm/packaging/krunner-llm.spec; \
    ver=$(rpmspec -q --srpm --qf '%{version}' "$spec"); \
    cp -a /src/krunner-llm "/src/krunner-llm-$ver"; \
    tar -czf "$HOME/rpmbuild/SOURCES/krunner-llm-$ver.tar.gz" \
        --exclude=.git --exclude='build*' -C /src "krunner-llm-$ver"; \
    rpmbuild -ba "$spec"

# Copy the finished RPMs into the mounted output directory.
VOLUME /out
CMD ["sh", "-c", "cp -v /root/rpmbuild/RPMS/*/*.rpm /root/rpmbuild/SRPMS/*.rpm /out/"]
