class Yabai < Formula
  desc "Tiling window manager with native macOS tab support"
  homepage "https://github.com/toby-wong/yabai"
  url "https://github.com/toby-wong/yabai/releases/download/toby-v7.1.25-r19.1/yabai-v7.1.25-toby.19.1.tar.gz"
  version "7.1.25.19.1"
  sha256 "6e62a399e83a37a4fa7b298be0fe31d053b000edac71c2c99078fb74c9584451"
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
    assert_match "yabai-v7.1.25-toby.19.1+f83a7288", shell_output("#{bin}/yabai --version")
  end
end
