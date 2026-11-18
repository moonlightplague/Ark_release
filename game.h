#pragma once
// ============================================================
//  game.h —— ⚙️ 后端·规则（游戏「怎么运转」，与画面无关）
// ============================================================
//  拆分后的第 2 个模块，是整个游戏的「大脑」：血量、伤害、索敌、
//  移动、部署、撤退、胜负……全在这。
//
//  【关键约定】后端不画任何东西、不持有任何贴图/窗口。
//    - 它只用到 SFML 的「数据类型」：向量(坐标)、颜色、矩形。
//    - 所以这里 include 的是 <SFML/Graphics/Color.hpp> 这种小头文件，
//      而不是整个 <SFML/Graphics.hpp>。sf::Color 只是 4 个字节的 RGBA 数据，
//      不是贴图。
//    - 画面部分全在 render.h/.cpp；render 认识 game，game 不认识 render
//      （「模型不依赖视图」）。
//
//  类的方法：类里只「声明」，实现放到 game.cpp —— 这就是「分离编译」。
//
//  【M3 学生版】下面的数据表里含 ★任务⑤（新增干员艾雅法拉）；完整答案见 game.h.answer。
// ============================================================

#include "config.h"
#include <SFML/System/Vector2.hpp>
#include <SFML/Graphics/Color.hpp>   // sf::Color：4 字节 RGBA 数据（不是贴图）
#include <SFML/Graphics/Rect.hpp>    // sf::FloatRect：矩形数据（点击判定 / 按钮几何）
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>


// ---------- 地图（关卡数据）----------
//   'S' 出怪口   'E' 终点（蓝门）   '#' 怪物路径（也是「地面」：近战/重装可部署）
//   'P' 高台（怪不能走，可放远程：狙击/术师）   '.' 不可走、不可部署
//   全程只读 → 用 const（命名空间 const 是内部链接，放头文件安全、不需 extern）。
const std::vector<std::string> map = {
    ".PPPPPPP.",
    "E#P#####.",
    ".###P###S",
    ".PPPPPPP.",
};


// ---------- 战斗属性 + 伤害公式 ----------

// 一个单位的战斗属性。Enemy 和 Operator 各存一份——这只是共享的「数据包」，不是继承。
struct CombatStats {
    int   atk;          // 攻击力
    float interval;     // 攻击间隔（秒）
    int   rangeRadius;  // 攻击范围半径：0=本格1x1, 1=3x3, 2=5x5
    bool  magic;        // 攻击类型：true=法术, false=物理
    int   def;          // 防御力
    float resist;       // 魔法抗性（0..1）
};

// 伤害公式（工具，已给）：物理 = 攻击力 - 目标防御；法术 = 攻击力 ×(1 - 目标法抗)；都不低于攻击力的 5%。（体见 game.cpp）
int computeDamage(const CombatStats& a, const CombatStats& d);


// ---------- 敌人数据表（仿 OP_DEF：一敌一行）----------
// 敌人：源石虫 / 士兵。加敌人只加一行：名字 / 美术文件名 / 血量 / 移速 / 战斗属性。
const int ENEMY_TYPES = 2;
const int ENEMY_SLUG = 0, ENEMY_SOLDIER = 1;

struct EnemyDef {
    const char* name;      // 敌人名（源石虫/士兵）——自文档，暂不显示
    const char* asset;     // 美术文件名：graphics/enemy/<asset>.png（前端 loadAssets 用）
    int         maxHp;     // 最大血量
    float       stepTime;  // 每格用时(秒)：越小走得越快
    sf::Color   color;     // 缺图兜底色（前端画不出贴图时用；和干员的 OP_DEF.color 对称）
    CombatStats cs;        // 战斗属性（攻击力/间隔/范围/类型/防御/法抗）
};

// 下标 = 敌人编号 type。数值与原来的 maxHpOf/stepTimeOf/enemyStats 完全一致。
const EnemyDef ENEMY_DEF[ENEMY_TYPES] = {
    { "源石虫", "slug",    550,  1.7f, sf::Color(220,  60,  60), { 130, 1.7f, 0, false,   0, 0.20f } },  // ENEMY_SLUG
    { "士兵",   "soldier", 1650, 2.3f, sf::Color(150, 150, 160), { 200, 2.0f, 0, false, 100, 0.00f } },  // ENEMY_SOLDIER
};

// 干员编号（type，= OP_DEF 的下标；按干员名命名）。
const int OP_COUNTS  = 3;   // ★M3 任务⑤：加入艾雅法拉后，这里改回 4
const int OP_RANGERS = 0, OP_NOIRCORNE = 1, OP_DURIN = 2;   // ★任务⑤：再补一个 OP_EYJAFJALLA = 3

// 职业常量（干员的「职业」；OP_DEF.role 用它，canPlaceHere 也据此定站位）。
const int OP_SNIPER = 0, OP_DEFENDER = 1, OP_CASTER = 2;

// ---------- 干员数据表 ----------
// 一个干员的核心数据：名字/职业/美术前缀/费用/血量/战斗属性/弹道旋转。
//   美术全部按名派生：operator/<asset>_fig|_model.png、弹道 effect/<asset>_hit.png；
//   主题色（UI）按【职业】在前端取（见 render.cpp 的 ROLE_COLOR）——数据表里不放任何美术/颜色信息。
//   所以加干员＝加这一行 + 三张按名命名的图，前端一行不用改。
struct OperatorDef {
    const char* name;    // 干员名（巡林者/黑角/杜林/艾雅法拉）
    int         role;    // 职业：OP_SNIPER / OP_DEFENDER / OP_CASTER（canPlaceHere 定站位；前端按它取主题色）
    const char* asset;   // 美术前缀：operator/<asset>_fig|_model.png、弹道 effect/<asset>_hit.png（前端全按它派生）
    int         cost;    // 部署费用
    int         maxHp;   // 最大血量
    CombatStats cs;      // 战斗属性（远程 = rangeRadius>0，会发弹）
    bool        spin;    // 弹道是否朝飞行方向旋转（箭矢=true / 法球=false）
    int         block;   // 阻挡数：能同时挡住几个敌人（黑角=3，其余=1）
};

// 干员数据表（下标 = 干员编号 type）。role=职业常量（canPlaceHere 用它定站位、前端用它取主题色）；spin=弹道是否旋转；block=阻挡数。
const OperatorDef OP_DEF[OP_COUNTS] = {
    { "巡林者",   OP_SNIPER,   "rangers",    15, 1000, { 200, 1.7f, 2, false,  50, 0.00f }, true,  1 },  // OP_RANGERS（狙击→高台，箭矢转向）
    { "黑角",     OP_DEFENDER, "noircorne",  10, 2400, { 100, 2.5f, 0, false, 150, 0.00f }, false, 3 },  // OP_NOIRCORNE（重装→地面，近战无弹道，挡 3 个）
    { "杜林",     OP_CASTER,   "durin",      20, 1300, { 150, 2.0f, 1, true,    0, 0.00f }, false, 1 },  // OP_DURIN（术师→高台，法球不转）
    // ★★★ M3 任务⑤：在这里加入第 4 个干员「艾雅法拉」（术师，走高台，发不旋转的火球）★★★
    //   步骤：① 上面 OP_COUNTS 改成 4、补常量 OP_EYJAFJALLA = 3；② 在此加一行 OperatorDef 数据
    //   （职业 OP_CASTER、asset "eyjafjalla"、自拟费用/血量/战斗属性/阻挡数）。
    //   美术按名派生：放好 graphics/operator/eyjafjalla_{fig,model}.png 与 graphics/effect/eyjafjalla_hit.png 即可，前端一行不用改。
};


// 一枚飞行中的弹体（纯视觉）：从 from 直线飞到 to，t 到 dur 就消失。
// 后端只负责「记下这一枪从哪飞到哪、是哪种」，怎么画由前端管。
struct Projectile {
    sf::Vector2f from, to;   // 起点(干员) / 终点(开火时目标格中心)
    float t = 0.f;           // 已飞时间
    float dur = 0.15f;       // 总飞行时间 = 距离 / PROJECTILE_SPEED
    int   opType = 0;        // 哪个干员发的（前端据此取 <asset>_hit 贴图 / 职业色 / 是否旋转）
};


// ============================================================
//  class Enemy —— 一个敌人（数据 + 移动逻辑；怎么画在 render.cpp）
// ============================================================
class Enemy {
public:
    int   row, col;          // 当前所在格
    int   prevRow, prevCol;  // 上一格（平滑滑行时从这里插值过来）
    int   type;              // 敌人种类：ENEMY_SLUG（源石虫）/ ENEMY_SOLDIER（士兵）
    int   id;                // 稳定编号：堆在同一格时用来左右错开，避免糊在一起
    int   hp;                // 当前血量（实时；被攻击时减少，血条会跟着变）
    int   maxHp;             // 最大血量（来自 ENEMY_DEF，画血条用）
    CombatStats cs;          // 战斗属性（攻击力/间隔/范围/类型/防御/法抗）
    float atkTimer;          // 攻击计时器：攒够 cs.interval 就打一次
    float atkAnim;           // 攻击动画剩余时间（>0 时贴图摇一下）
    bool  alive;             // 是否还在场上
    bool  blocked;           // 本帧是否被干员挡住（用于绘制时下移/错开）
    float stepTime;          // 每格用时(秒)：越小走得越快（移动速度，来自 ENEMY_DEF）
    float step;              // 走格计时器；<0 = 还没进场（错开出场）

    // 每格用时：spawnWaves 排错峰出场时用（实例还没造出来，按 type 查表）。（体见 game.cpp）
    static float stepTimeOf(int t);

    // 构造：从 ENEMY_DEF[t] 取该种敌人的血量/移速/战斗属性来初始化（体见 game.cpp）。
    Enemy(int r, int c, int t, int id_, float startDelay);

    bool onField() const;    // 是否已进场（step>=0；<0 = 还在场外排队错峰出场）（体见 cpp）

    // 移动一步（实现见 game.cpp）。blocked=true（同格有干员挡住）时停在原地专心打、不前进。
    // 走到终点 E 返回 true（这只漏进了蓝门）。
    bool update(float dt, bool blocked);

    // —— 一个敌人「对自己做的事」（封装：自己管自己的血量/攻击节奏；体全见 game.cpp）——
    float hpRatio() const;                    // 血条比例 = hp/maxHp（画血条用）
    void  takeDamage(const CombatStats& atk); // 挨一下：按自身防御/法抗算实伤，血空则 alive=false
    bool  attackReady(float dt);              // 衰减出手动画 + 推进攻击计时器；攒够 cs.interval 返回 true
    void  onAttack();                         // 出手了：计时器归零 + 触发贴图摇一下
    void  holdAttack();                       // 没目标：计时器保持就绪，一有目标立刻打
};


// ============================================================
//  class Operator —— 一个干员（数据 + 查询；怎么画在 render.cpp）
// ============================================================
class Operator {
public:
    // 成员都带默认值：即使构造函数体是空的（学生还没写），对象也是「合法、不崩」的——
    // 干员会落在默认格 (0,0)、100 血。学生的任务就是在构造函数里用 OP_DEF[t] 覆盖这些默认值。
    int row = 0, col = 0, id = 0;                 // 所在格 / 干员编号（默认落在 (0,0)、编号 0）
    int hp = 100;                                   // 当前血量（实时；被敌人攻击时减少）
    int maxHp = 100;                                // 最大血量（正确值来自 OP_DEF，不由职业推）
    int cost = 10;                                  // 部署费用（同上）
    const char* name = "?";                         // 干员名（同上）
    CombatStats cs = { 10, 5.f, 1, false, 0, 0.f }; // 战斗属性（正确值来自 OP_DEF）
    float atkTimer = 0.f;                           // 攻击计时器
    float atkAnim = 0.f;                            // 攻击动画剩余时间（>0 时立绘摇一下）
    int   blockCount = 1;                           // 阻挡数：能同时挡几个敌人（正确值来自 OP_DEF；黑角=3，其余=1）
    int   blockUsed = 0;                            // 本帧已挡住的敌人数（引擎每帧重置，不是构造时初始化的）

    // 构造：从 OP_DEF[op_id] 取该干员的血量/费用/名字/战斗属性/阻挡数来初始化（体见 game.cpp；可对照 Enemy 的构造函数），放在r行c列。
    Operator(int r, int c, int op_id);

    // 按干员编号 op_id 查数据（部署前还没有实例时用；都从 OP_DEF 取，不是按职业写死；体见 game.cpp）。
    static int         costOf(int op_id);   // 部署费用
    static const char* nameOf(int op_id);   // 干员名
    static int         role  (int op_id);   // 职业（OP_SNIPER/DEFENDER/CASTER）

    // 敌人 e 是否落在本干员攻击范围内（Chebyshev 方形：横竖距离都 ≤ 射程半径）（体见 game.cpp）。
    bool  inRange(const Enemy& e) const;

    // —— 一个干员「对自己做的事」（封装：自己管自己的血量/攻击节奏；体全见 game.cpp）——
    float hpRatio() const;                    // 血条比例 = hp/maxHp（画血条用）
    void  takeDamage(const CombatStats& atk); // 挨一下：按自身防御/法抗算实伤
    bool  attackReady(float dt);              // 衰减出手动画 + 推进攻击计时器；攒够 cs.interval 返回 true
    void  onAttack();                         // 出手了：计时器归零 + 触发立绘摇一下
    void  holdAttack();                       // 没目标：计时器保持就绪，一有目标立刻打
};


// ---------- 界面几何（纯数学：给定编号/格子算出屏幕矩形）----------
// 放在后端是因为「点击判定」要用；前端画按钮时也调它，两边共用同一份坐标，保证点哪画哪一致。

// 第 op_id 张干员卡在底部部署栏里的矩形。
inline sf::FloatRect deployCardRect(int op_id)
{
    float cardW = float(DEPLOY_H) - 12.f;
    float gap   = 10.f;
    float x = 12.f + op_id * (cardW + gap);
    float y = float(BOARD_H) + 6.f;
    return sf::FloatRect({x, y}, {cardW, cardW});
}

// 撤退按钮的矩形：浮在 (row,col) 干员头顶。
inline sf::FloatRect retreatButtonRect(int row, int col)
{
    const float bs = 46.f;                    // 按钮边长
    sf::Vector2f c = cellCenter(row, col);
    float top = c.y - CELL_H * 0.45f - bs / 2.f;   // 抬到干员头顶附近
    if (top < 2.f) top = 2.f;                 // 别顶出画面
    return sf::FloatRect({c.x - bs / 2.f, top}, {bs, bs});
}


// 关卡可调参数 GameConfig 已挪到 config.h（本局初始费用/回费/生命在那里调）。


// ============================================================
//  class Game —— 游戏状态 + 敌人/干员集合 + 编排（全部逻辑，无绘制）
// ============================================================
class Game {
public:
    int  dp, life;             // 初始值来自 GameConfig（见构造函数）
    int  wave = 0, remainingEnemies = 0;
    int  enemiesTotal = 0;      // 本关敌人总数（用于「已击杀/总数」显示；击杀=离场=总数-剩余）
    bool gameOver = false, victory = false;
    int  selectedId = -1;     // 当前选中的干员卡（-1 = 未选）
    int  retreatRow = -1, retreatCol = -1;   // 正弹出撤退按钮的干员所在格（-1 = 没有）
    float dpTimer = 0.f;
    float dpInterval;           // 每隔多少秒 +1 DP（来自 GameConfig）
    float opCooldown[OP_COUNTS] = {};   // 每种干员的再部署冷却（秒），>0=冷却中（长度随 OP_COUNTS，加干员不用改这里）

    std::vector<Enemy>      enemies; //存储所有场上的敌人
    std::vector<Operator>   operators; //存储所有场上的干员
    std::vector<Projectile> projectiles;   // 飞行中的弹体（纯视觉）

    // 用一组参数开局（不传则全用默认）。先把参数存进成员，再排出怪。
    Game(GameConfig cfg = {})
        : dp(cfg.startDp), life(cfg.startLife), dpInterval(cfg.dpInterval)
    {
        spawnWaves();
    }

    // —— 逻辑主流程（实现见 game.cpp）——
    void spawnWaves();
    void update(float dt);
    void resolveCombat(float dt);
    void cullDead();

    // —— 查询 ——
    Operator* operatorAt(int row, int col);
    Enemy*    findEnemyInRange(const Operator& op);
    const Operator* selectedOperator() const;   // 当前被点选（显示信息卡/撤退按钮）的干员；供前端读

    // —— 输入 ——
    void handleClick(float mx, float my);   // mx,my 已是逻辑坐标（main 里换算好）

    // —— 放置 / 撤退规则 ——
    bool canPlaceHere(int row, int col, int op_id) const;
    bool isCellOccupied(int row, int col) const;
    bool isOpDeployed(int op_id) const;
    bool tryPlaceOperator(int row, int col, int op_id);
    bool retreatAt(int row, int col);
};
