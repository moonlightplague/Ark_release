#pragma once
// ============================================================
//  render.h —— 🎨 前端·美术（游戏「长什么样」）对外的极简接口
// ============================================================
//  前端认识后端：这里 include "game.h"，因为画面要读 Game 的状态。
//  但后端【绝不】反过来 include 本文件 —— 「模型不依赖视图」。
//
//  对外只暴露 3 个函数，main 用它们就够了；所有贴图、所有具体的
//  drawXxx 都是 render.cpp 内部的私有(static)细节，外面看不到、也不用管。
// ============================================================

#include <SFML/Graphics.hpp>
#include "game.h"

// 载入字体 + 所有美术素材（在 main 开场调用一次）。缺图自动兜底。
void loadAssets();

// 把整个游戏画面画出来（地图/单位/弹道/UI…）。只读 Game，不改它。
void render(sf::RenderWindow& win, const Game& game);

// 让固定逻辑视图在窗口里保持宽高比：多出来的边留黑边(letterbox)，而不是拉伸变形。
void applyLetterbox(sf::View& view, sf::Vector2u winSize);
