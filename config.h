#pragma once
// ============================================================
//  config.h —— 🎛️ 可调旋钮（本项目所有「数字常量」都在这）
// ============================================================
//  拆分后的第 1 个模块。想调游戏平衡（血量条尺寸、回费速度、冷却、
//  动画快慢、弹速…），基本只用改这个文件，不必碰逻辑或美术代码。
//
//  这里全是 constexpr / const / inline —— 命名空间下的 const 是「内部链接」，
//  放在头文件里被多个 .cpp 各自包含也不会重定义，所以【不需要 extern】。
//  （这正是我们按「前端/后端」拆分能躲开 extern 的第一步。）
//
//  只依赖 SFML 的向量类型（数学，不是美术）：cellCenter 返回一个坐标。
// ============================================================

#include <SFML/System/Vector2.hpp>

// ---------- 棋盘 / 网格 ----------
constexpr int GRID_W = 9;    // 地图列数（对应关卡图）
constexpr int GRID_H = 4;    // 地图行数

constexpr int   BOARD_W = 800;                     // 棋盘显示尺寸（16:9，配合关卡图）
constexpr int   BOARD_H = 450;
constexpr float CELL_W  = float(BOARD_W) / GRID_W; // ≈ 88.9（非正方格没关系）
constexpr float CELL_H  = float(BOARD_H) / GRID_H; // = 112.5
constexpr int   HUD_W    = 200;   // 右侧信息栏宽度
constexpr int   DEPLOY_H = 96;    // 底部「部署栏」高度

// ---------- 背景图裁剪（让 9×4 逻辑网格对齐关卡图里的地板）----------
//  关卡图 graphics/map/main_00-01.png 是一张【透视渲染】：真正能走的地板只占画面中间，
//  四周是管道/箱子/空调等装饰。若整张图直接铺满棋盘，网格会压到这些非地板区域。
//  于是 drawMap 先按下面的裁剪框只取「地板那块」，再拉伸铺满棋盘 —— 网格就落在地板上。
//  （透视图无法让两个门都完美对齐，取折中即可；想微调就改这 4 个数。offline 预览法见 M4。）
//  取值依据：把关卡图里真实地板的透视梯形四角取平均矩形，让【中间两行】(路径/单位所在) 对得最准。
constexpr int MAP_CROP_X = 137, MAP_CROP_Y = 135;   // 裁剪框左上角（像素，基于 1280×720 原图）
constexpr int MAP_CROP_W = 955, MAP_CROP_H = 393;   // 裁剪框宽、高

// ---------- 时间 / 冷却 / 动画（秒或角度）----------
// 注：DP 回费速度等本局关卡参数已下放为本文件末尾的 GameConfig（Game 构造用它）。
const float WAVE_GAP  = 5.0f;   // 两波之间的时间间隔（秒，可调）
const float DEPLOY_COOLDOWN = 30.f;   // 干员撤退/倒下后，多少秒才能再部署
const float ATK_ANIM_TIME  = 0.18f;   // 攻击动画时长（秒）
const float ATK_WOBBLE_DEG = 12.f;    // 攻击时贴图最大摇摆角度
const float PROJECTILE_SPEED = 1000.f;      // 弹体飞行速度（像素/秒，决定飞行时长）
const float SNIPER_ARROW_BASE_DEG = 45.f;   // 箭矢贴图原生朝向修正（右上≈-45° → +45° 对齐飞行方向）


// ---------- 本局关卡参数（在这里调本关的初始费用/回费/生命）----------
// Game 构造用它开局（main.cpp 里 `Game game;` 即用下面这些默认值）。纯 int/float，无库依赖。
struct GameConfig {
    int   startDp    = 10;     // 初始部署费用
    float dpInterval = 1.0f;   // 每隔多少秒回 1 点部署费（越小 = 回费越快）
    int   startLife  = 4;      // 初始生命值
};

// 格子(行,列) → 该格中心的像素坐标。纯数学，前后端都会用到，所以放在最底层的 config。
inline sf::Vector2f cellCenter(int row, int col)
{
    return sf::Vector2f((col + 0.5f) * CELL_W, (row + 0.5f) * CELL_H);
}
