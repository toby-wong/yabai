class Yabai < Formula
  desc "Tiling window manager with native macOS tab support"
  homepage "https://github.com/toby-wong/yabai"
  url "https://github.com/toby-wong/yabai/releases/download/toby-v7.1.25-r33.1/yabai-v7.1.25-toby.33.1.tar.gz"
  version "7.1.25.33.1"
  sha256 "e0e90a7c9102e7d5332b71cf56d526aba39a93452bfc4a6ef74b5ad68b045934"
  license "MIT"

  depends_on macos: :big_sur

  def install
    bin.install "bin/yabai"
    man1.install "doc/yabai.1"
    (pkgshare/"examples").install "examples/yabairc", "examples/skhdrc"
    pkgshare.install "build-info.json", "FORK.md"
  end

  def caveats
    <<~EOS
      This formula installs the prebuilt Apple Silicon/Intel release.
      Enable native tabs in your yabairc:
        yabai -m config window_native_tabs on

      Start at login: yabai --start-service
      After upgrading: yabai --restart-service
      Grant Accessibility access to yabai if macOS requests it.
    EOS
  end

  test do
    assert_match "yabai-v7.1.25-toby.33.1+1f2daf70", shell_output("#{bin}/yabai --version")
  end
end
