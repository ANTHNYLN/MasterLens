#!/bin/bash
# MasterLens 卸载脚本（双击运行）
set -u

VST3="$HOME/Library/Audio/Plug-Ins/VST3/MasterLens.vst3"
AU="$HOME/Library/Audio/Plug-Ins/Components/MasterLens.component"
APP="$HOME/Applications/MasterLens.app"

echo "MasterLens 卸载程序"
echo "========================================"

for target in "$VST3" "$AU" "$APP"; do
    if [ -e "$target" ]; then
        echo "→ 删除 $target"
        rm -rf "$target"
    fi
done

echo "========================================"
echo "已卸载。如果 DAW 仍在缓存插件列表，请重启 DAW。"
echo ""
read -n 1 -s -r -p "按任意键关闭窗口..."
echo
