class Yabai < Formula
  desc "Tiling window manager with native macOS tab support"
  homepage "https://github.com/toby-wong/yabai"
  url "https://github.com/toby-wong/yabai/releases/download/toby-v7.1.25-r31.1/yabai-v7.1.25-toby.31.1.tar.gz"
  version "7.1.25.31.1"
  sha256 "7841479416d7ef0e50b853ce0d8c6210a289e493a28d0c41aa3fcaaa385c2d8e"
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
    assert_match "yabai-v7.1.25-toby.31.1+25a50843", shell_output("#{bin}/yabai --version")
  end
end
