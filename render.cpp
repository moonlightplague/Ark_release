// ============================================================
//  render.cpp —— 🎨 前端·美术的「实现」（所有贴图 + 所有绘制）
// ============================================================
//  【本项目为什么不用 extern？】
//    可变的全局只有「贴图 / 字体」。它们只被绘制代码用到，于是我们把
//    它们全部关在这一个 .cpp 里、写成 static（文件私有）。既然它们
//    永远不跨文件，别的文件就不需要 extern 去声明它们 —— 一个都不用。
//    这就是「前端/后端分离」顺手带来的好处。
//
//    前端读后端的数据（Game/Enemy/Operator、OP_DEF、map…），
//    但后端对本文件一无所知。
// ============================================================

#include "render.h"
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>


// ============================================================
//  美术资源（全部 static：文件私有，外面看不到 → 不需要 extern）
// ============================================================
static sf::Font    font;
static bool        hasFont = false;
static sf::Texture levelTex;     // 关卡背景图
static bool        hasLevelTex = false;
static sf::Texture escapeTex;    // 撤退按钮图标（graphics/ui/escape.png）
static bool        hasEscapeTex = false;
static sf::Texture hpBackTex, hpFillTex;   // 血条：空条 / 满条
static bool        hasHpBack = false, hasHpFill = false;
static sf::Texture costTex;                // 部署费用图标
static bool        hasCostTex = false;

// 敌人贴图。文件名来自 ENEMY_DEF[type].asset（graphics/enemy/<asset>.png）；都 512×512 填满画布 → 同款绘制。
static sf::Texture enemyTex[ENEMY_TYPES];
static bool        hasEnemyTex[ENEMY_TYPES] = {};

// 干员贴图：_avatar 头像（部署卡）/ _model Q 版（地图上）/ _hit 弹道（远程开火特效）。全部前缀 = OP_DEF[op_id].asset。
static sf::Texture opAvatarTex[OP_COUNTS];   // 部署卡头像 operator/<asset>_avatar.png（256² 方图，填满卡片）
static sf::Texture opModelTex[OP_COUNTS];
static sf::Texture opHitTex[OP_COUNTS];      // 弹道贴图 effect/<asset>_hit.png（近战干员没有，留空）
static bool hasOpAvatar[OP_COUNTS]   = {};
static bool hasOpModel[OP_COUNTS] = {};
static bool hasOpHit[OP_COUNTS]   = {};


// 根据地形字符返回「叠加色」——半透明，盖在背景图上，让背景氛围透出来。（纯视觉映射，归前端）
static sf::Color tileOverlayColor(char t)
{
    switch (t)
    {
        case '#': return sf::Color(235, 215, 150,  55);  // 道路(可走)：暖色薄雾
        case 'S': return sf::Color(220,  60,  60, 120);  // 出怪口（红门）
        case 'E': return sf::Color( 60, 140, 230, 130);  // 终点（蓝门）
        case 'P': return sf::Color( 90, 150, 230,  70);  // 高台
        default:  return sf::Color( 10,  12,  20, 120);  // 地面 '.'：压暗
    }
}

// 职业主题色（前端）：每个职业一个色，干员的 UI 色全按它的【职业】取——干员数据表里不放任何颜色。
// 下标 = 职业常量 OP_SNIPER / OP_DEFENDER / OP_CASTER。加干员不用碰这里（同职业自动同色）。
static const sf::Color ROLE_COLOR[] = {
    sf::Color( 80, 170, 255),   // OP_SNIPER   狙击：蓝
    sf::Color(240, 200,  60),   // OP_DEFENDER 重装：黄
    sf::Color(200,  90, 230),   // OP_CASTER   术师：紫
};
// 某个干员(type)的主题色 = 它职业的主题色。
static sf::Color operatorColor(int op_id) { return ROLE_COLOR[OP_DEF[op_id].role]; }

// 职业信息（前端）：也按【职业】取，下标 = 职业常量 OP_SNIPER / OP_DEFENDER / OP_CASTER。
//   ROLE_NAME —— 信息卡右上角显示的职业名（例如「重装干员」）。
//   ROLE_ICON —— 职业图标文件名：graphics/ui/class_<icon>.png，部署卡右上角显示。
// 加干员不用碰这里（同职业自动同名同图标）。
static const int    ROLE_COUNTS  = 3;   // 职业数
static const char*  ROLE_NAME[ROLE_COUNTS] = { "狙击干员", "重装干员", "术师干员" };
static const char*  ROLE_ICON[ROLE_COUNTS] = { "sniper",   "defender", "caster"   };
static sf::Texture  classTex[ROLE_COUNTS];
static bool         hasClassTex[ROLE_COUNTS] = {};


// ============================================================
//  资源载入
// ============================================================

// 依次尝试几个常见字体路径；找到就能画 HUD 文字。
static bool loadAnyFont()
{
    const char* candidates[] = {
        "assets/font.otf",                                  // 自备（推荐，放一个含中文的字体；与 M1/M4 一致）
        "C:/Windows/Fonts/msyh.ttc",                        // Windows 微软雅黑（含中文）
        "C:/Windows/Fonts/arial.ttf",                       // Windows 英文
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",  // Linux
        "/System/Library/Fonts/Supplemental/Arial.ttf",     // macOS
    };
    for (const char* p : candidates)
        if (font.openFromFile(p)) return true;
    std::cerr << "[提示] 未找到字体，HUD 文字暂不显示。把任意字体放到 assets/font.otf 即可。\n";
    return false;
}

// 载入字体 + 美术素材到（本文件的）全局。失败自动兜底。
void loadAssets()
{
    hasFont = loadAnyFont();
    hasLevelTex = levelTex.loadFromFile("graphics/map/main_00-01.png");
    if (!hasLevelTex)
        std::cerr << "[提示] 未找到关卡背景图 graphics/map/main_00-01.png，改用纯色格子。\n";
    for (int type = 0; type < ENEMY_TYPES; type++)   // 逐种加载敌人贴图，缺图兜底色块
    {
        std::string path = std::string("graphics/enemy/") + ENEMY_DEF[type].asset + ".png";
        hasEnemyTex[type] = enemyTex[type].loadFromFile(path);
        if (!hasEnemyTex[type])
            std::cerr << "[提示] 未找到敌人贴图 " << path << "，改用色块。\n";
    }
    hasEscapeTex = escapeTex.loadFromFile("graphics/ui/escape.png");
    if (!hasEscapeTex)
        std::cerr << "[提示] 未找到撤退按钮图标 graphics/ui/escape.png，改用文字「撤」。\n";
    hasHpBack = hpBackTex.loadFromFile("graphics/ui/slider_hp_back.png");
    hasHpFill = hpFillTex.loadFromFile("graphics/ui/slider_hp_fill.png");
    if (!hasHpBack || !hasHpFill)
        std::cerr << "[提示] 未找到血条贴图 graphics/ui/slider_hp_back/fill.png，改用纯色条。\n";
    hasCostTex = costTex.loadFromFile("graphics/ui/img_cost.png");
    if (!hasCostTex)
        std::cerr << "[提示] 未找到费用图标 graphics/ui/img_cost.png，费用只显示数字。\n";
    for (int role = 0; role < ROLE_COUNTS; role++)   // 职业图标，按职业名派生 graphics/ui/class_<icon>.png
    {
        std::string path = std::string("graphics/ui/class_") + ROLE_ICON[role] + ".png";
        hasClassTex[role] = classTex[role].loadFromFile(path);
        if (!hasClassTex[role])
            std::cerr << "[提示] 未找到职业图标 " << path << "，部署卡不显示职业图标。\n";
    }
    // 干员美术全按 OP_DEF[op_id].asset 派生：operator/<asset>_avatar|_model.png + effect/<asset>_hit.png（弹道）。
    for (int op_id = 0; op_id < OP_COUNTS; op_id++)
    {
        std::string base = std::string("graphics/operator/") + OP_DEF[op_id].asset;
        hasOpAvatar[op_id] = opAvatarTex[op_id].loadFromFile(base + "_avatar.png");   // 部署卡头像
        hasOpModel[op_id] = opModelTex[op_id].loadFromFile(base + "_model.png");
        if (!hasOpAvatar[op_id] || !hasOpModel[op_id])
            std::cerr << "[提示] 干员素材缺失：" << base << "_avatar/_model.png\n";
        // 弹道贴图：只有远程干员(rangeRadius>0)才有 _hit；近战干员不去加载（免得 SFML 报找不到图）。
        if (OP_DEF[op_id].cs.rangeRadius > 0)
        {
            hasOpHit[op_id] = opHitTex[op_id].loadFromFile(
                std::string("graphics/effect/") + OP_DEF[op_id].asset + "_hit.png");
            if (!hasOpHit[op_id])
                std::cerr << "[提示] 远程干员弹道贴图缺失：graphics/effect/" << OP_DEF[op_id].asset << "_hit.png，改用小圆点。\n";
        }
    }
}


// ============================================================
//  基础绘制块
// ============================================================

// 在 (cx, topY) 处画一条血条：空条铺底，满条按 frac(0..1) 从左裁剪。敌人/干员共用。
static void drawHpBar(sf::RenderWindow& win, float cx, float topY, float frac)
{
    if (frac < 0.f) frac = 0.f;
    if (frac > 1.f) frac = 1.f;
    const float barW = CELL_W * 0.66f, barH = 7.f;
    float left = cx - barW / 2.f;

    if (hasHpBack && hasHpFill)
    {
        sf::Sprite back(hpBackTex);                       // 空条（背景）铺满整条
        sf::Vector2u bs = hpBackTex.getSize();
        back.setScale(sf::Vector2f{ barW / float(bs.x), barH / float(bs.y) });
        back.setPosition(sf::Vector2f{ left, topY });
        win.draw(back);

        sf::Sprite fill(hpFillTex);                       // 满条（前景）按血量比例从左裁剪
        sf::Vector2u fs = hpFillTex.getSize();
        int clipW = int(fs.x * frac);
        if (clipW < 1 && frac > 0.f) clipW = 1;
        fill.setTextureRect(sf::IntRect({0, 0}, {clipW, int(fs.y)}));
        fill.setScale(sf::Vector2f{ barW / float(fs.x), barH / float(fs.y) });  // 与空条同缩放→裁多少显多少
        fill.setPosition(sf::Vector2f{ left, topY });
        win.draw(fill);
    }
    else   // 兜底：灰底 + 绿条
    {
        sf::RectangleShape bg(sf::Vector2f{barW, barH});
        bg.setPosition(sf::Vector2f{left, topY});
        bg.setFillColor(sf::Color(40, 40, 48, 220));
        win.draw(bg);
        sf::RectangleShape fg(sf::Vector2f{barW * frac, barH});
        fg.setPosition(sf::Vector2f{left, topY});
        fg.setFillColor(sf::Color(80, 200, 90));
        win.draw(fg);
    }
}


// ============================================================
//  单位绘制（原本是 Enemy::draw / Operator::draw，拆分时搬到前端来）
// ============================================================

// 一个敌人：平滑插值 + 贴图 + 攻击摇晃 + 血条。
static void drawEnemy(sf::RenderWindow& win, const Enemy& e)
{
    if (!e.alive || !e.onField()) return;
    float t = e.step / e.stepTime;   // 0..1：本格走了多少
    if (t > 1.f) t = 1.f;
    sf::Vector2f from   = cellCenter(e.prevRow, e.prevCol);
    sf::Vector2f to     = cellCenter(e.row, e.col);
    sf::Vector2f center = from + (to - from) * t;

    if (e.blocked)   // 被干员挡住：站到干员前方 + 按 id 左右错开，避免和干员/其他敌人糊在一起
    {
        center.x += ((e.id % 3) - 1) * CELL_W * 0.24f;   // -1 / 0 / +1 三档
        center.y += CELL_H * 0.22f;                      // 下移，别盖住干员上半身/血条
    }

    float wobble = (e.atkAnim > 0.f) ? ATK_WOBBLE_DEG * std::sin(3.14159265f * e.atkAnim / ATK_ANIM_TIME) : 0.f;
    if (hasEnemyTex[e.type])
    {
        sf::Sprite spr(enemyTex[e.type]);
        sf::Vector2u ts = enemyTex[e.type].getSize();
        float scale = (CELL_W * 0.95f) / float(ts.x);
        spr.setScale(sf::Vector2f(scale, scale));
        spr.setOrigin(spr.getLocalBounds().size / 2.f);
        spr.setPosition(center);
        spr.setRotation(sf::degrees(wobble));          // 攻击时摇一下
        win.draw(spr);
    }
    else
    {
        float radius = CELL_H * 0.3f;
        sf::CircleShape body(radius);
        body.setOrigin(sf::Vector2f(radius, radius));
        body.setFillColor(ENEMY_DEF[e.type].color);   // 缺图兜底色（数据驱动，见 ENEMY_DEF）
        body.setPosition(center);
        win.draw(body);
    }

    // 血条：画在贴图上方，长度 = 当前血量 / 最大血量
    drawHpBar(win, center.x, center.y - CELL_H * 0.4f, e.hpRatio());
}

// 一个干员：_model 立绘 + 攻击摇晃 + 头顶血条。
static void drawOperator(sf::RenderWindow& win, const Operator& o)
{
    sf::Vector2f center = cellCenter(o.row, o.col);
    float wobble = (o.atkAnim > 0.f) ? ATK_WOBBLE_DEG * std::sin(3.14159265f * o.atkAnim / ATK_ANIM_TIME) : 0.f;
    if (hasOpModel[o.id])
    {
        sf::Sprite m(opModelTex[o.id]);
        sf::Vector2u ts = opModelTex[o.id].getSize();
        float scale = (CELL_H * 1.7f) / float(ts.y);
        m.setScale(sf::Vector2f{scale, scale});
        m.setOrigin(m.getLocalBounds().size / 2.f);
        m.setPosition(center);
        m.setRotation(sf::degrees(wobble));            // 攻击时摇一下
        win.draw(m);
    }
    else
    {
        float s = CELL_H * 0.42f;
        sf::RectangleShape body(sf::Vector2f{s, s});
        body.setOrigin(sf::Vector2f{s / 2.f, s / 2.f});
        body.setFillColor(operatorColor(o.id));
        body.setOutlineThickness(-2.f);
        body.setOutlineColor(sf::Color(20, 22, 30, 220));
        body.setPosition(center);
        win.draw(body);
    }

    // 血条：抬到头顶上方（和被挡住、下移的敌人血条错开），并夹住不出界
    float barTop = center.y - CELL_H * 0.72f;
    if (barTop < 2.f) barTop = 2.f;
    drawHpBar(win, center.x, barTop, o.hpRatio());
}


// ============================================================
//  场景 / UI 绘制（原本是 Game::drawXxx，拆分时搬到前端、改成读 const Game&）
// ============================================================

static void drawMap(sf::RenderWindow& win)
{
    // 第 1 层：底（背景图 or 深色）
    if (hasLevelTex)
    {
        sf::Sprite bg(levelTex);
        bg.setTextureRect(sf::IntRect({MAP_CROP_X, MAP_CROP_Y}, {MAP_CROP_W, MAP_CROP_H}));    // 只取地板那块
        bg.setScale(sf::Vector2f(float(BOARD_W) / MAP_CROP_W, float(BOARD_H) / MAP_CROP_H));   // 裁剪块铺满棋盘
        bg.setPosition(sf::Vector2f(0.f, 0.f));
        win.draw(bg);

        sf::RectangleShape dim(sf::Vector2f{float(BOARD_W), float(BOARD_H)});  // 轻微压暗
        dim.setFillColor(sf::Color(0, 0, 0, 60));
        win.draw(dim);
    }
    else
    {
        sf::RectangleShape base(sf::Vector2f{float(BOARD_W), float(BOARD_H)});
        base.setFillColor(sf::Color(30, 34, 44));
        win.draw(base);
    }

    // 第 2 层：真实逻辑网格（半透明地形色 + 白色细边框）
    for (int r = 0; r < GRID_H; ++r)
        for (int c = 0; c < GRID_W; ++c)
        {
            sf::RectangleShape cell(sf::Vector2f(CELL_W, CELL_H));
            cell.setPosition(sf::Vector2f(c * CELL_W, r * CELL_H));
            cell.setFillColor(tileOverlayColor(map[r][c]));
            cell.setOutlineThickness(-1.f);
            cell.setOutlineColor(sf::Color(255, 255, 255, 50));
            win.draw(cell);
        }
}

// 右侧信息栏：标题 + 操作提示（部署费用/生命已搬到别处）。
static void drawHud(sf::RenderWindow& win)
{
    if (!hasFont) return;
    float hx = float(BOARD_W) + 12.f;
    auto line = [&](float y, const std::string& s, sf::Color col, unsigned size = 16) {
        sf::Text t(font, sf::String::fromUtf8(s.begin(), s.end()), size);
        t.setFillColor(col);
        t.setPosition(sf::Vector2f(hx, y));
        win.draw(t);
    };
    line(8,  "明日方舟",  sf::Color::White, 22);
    line(32, "Arknights", sf::Color(200, 205, 215), 15);
    line(52,  "· 点干员卡 → 选中",       sf::Color(185, 190, 200), 15);
    line(74,  "· 点格子 → 部署",         sf::Color(185, 190, 200), 15);
    line(95,  "   重装→地面 / 远程→高台", sf::Color(140, 145, 155), 13);
    line(122, "· 点场上干员 → 撤退按钮",  sf::Color(185, 190, 200), 15);
    line(144, "   再点按钮确认撤退",       sf::Color(140, 145, 155), 13);
}

// 正上方中间：进度（已清/总数）+ 剩余生命（叠在棋盘顶部，垫半透明底条便于看清）。
static void drawTopStatus(sf::RenderWindow& win, const Game& g)
{
    if (!hasFont) return;
    int killed = g.enemiesTotal - g.remainingEnemies;
    std::string s = "进度 " + std::to_string(killed) + "/" + std::to_string(g.enemiesTotal)
                  + "    生命 " + std::to_string(g.life);
    sf::Text t(font, sf::String::fromUtf8(s.begin(), s.end()), 20);
    t.setStyle(sf::Text::Bold);
    t.setFillColor(g.life > 1 ? sf::Color(235, 235, 240) : sf::Color(245, 90, 90));

    sf::FloatRect b = t.getLocalBounds();
    float cx = float(BOARD_W + HUD_W) / 2.f, cy = 18.f;
    t.setOrigin(sf::Vector2f{b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
    t.setPosition(sf::Vector2f{cx, cy});

    sf::RectangleShape bg(sf::Vector2f{b.size.x + 28.f, b.size.y + 12.f});
    bg.setOrigin(bg.getSize() / 2.f);
    bg.setPosition(sf::Vector2f{cx, cy});
    bg.setFillColor(sf::Color(0, 0, 0, 130));
    win.draw(bg);
    win.draw(t);
}

// 淡淡地把被点选干员的攻击范围格子高亮出来（画在单位之下）。
static void drawSelectedRange(sf::RenderWindow& win, const Game& g)
{
    const Operator* op = g.selectedOperator();
    if (!op) return;
    int rad = op->cs.rangeRadius;
    sf::Color col = operatorColor(op->id);
    for (int r = op->row - rad; r <= op->row + rad; ++r)
        for (int c = op->col - rad; c <= op->col + rad; ++c)
        {
            if (r < 0 || r >= GRID_H || c < 0 || c >= GRID_W) continue;
            sf::RectangleShape cell(sf::Vector2f{CELL_W, CELL_H});
            cell.setPosition(sf::Vector2f{c * CELL_W, r * CELL_H});
            cell.setFillColor(sf::Color(col.r, col.g, col.b, 55));      // 淡填充
            cell.setOutlineThickness(-1.f);
            cell.setOutlineColor(sf::Color(col.r, col.g, col.b, 120));
            win.draw(cell);
        }
}

// 被点选干员的信息小卡：名称 / 当前血量 / 攻击 / 防御 / 攻击类型（画在单位之上）。
static void drawSelectedInfo(sf::RenderWindow& win, const Game& g)
{
    const Operator* op = g.selectedOperator();
    if (!op || !hasFont) return;
    const float pw = 150.f, ph = 82.f;
    float px = (op->col + 1) * CELL_W + 6.f;                          // 优先放格子右侧
    if (px + pw > float(BOARD_W)) px = op->col * CELL_W - pw - 6.f;   // 放不下改左侧
    if (px < 4.f) px = 4.f;
    float py = op->row * CELL_H;
    if (py + ph > float(BOARD_H)) py = float(BOARD_H) - ph - 2.f;
    if (py < 2.f) py = 2.f;

    sf::RectangleShape card(sf::Vector2f{pw, ph});
    card.setPosition(sf::Vector2f{px, py});
    card.setFillColor(sf::Color(18, 20, 28, 215));
    card.setOutlineThickness(-2.f);
    card.setOutlineColor(operatorColor(op->id));
    win.draw(card);

    auto text = [&](const std::string& s, float oy, unsigned size, sf::Color c, bool bold) {
        sf::Text t(font, sf::String::fromUtf8(s.begin(), s.end()), size);
        t.setFillColor(c);
        if (bold) t.setStyle(sf::Text::Bold);
        t.setPosition(sf::Vector2f{px + 8.f, py + oy});
        win.draw(t);
    };
    text(std::string(op->name), 4.f, 16, sf::Color::White, true);

    // 右上角：职业信息（例如「重装干员」），用职业主题色、右对齐
    std::string roleName = ROLE_NAME[OP_DEF[op->id].role];
    sf::Text rt(font, sf::String::fromUtf8(roleName.begin(), roleName.end()), 13);
    rt.setFillColor(operatorColor(op->id));
    sf::FloatRect rb = rt.getLocalBounds();
    rt.setPosition(sf::Vector2f{px + pw - 8.f - (rb.position.x + rb.size.x), py + 8.f});
    win.draw(rt);

    text("血量 " + std::to_string(op->hp) + "/" + std::to_string(op->maxHp),
         26.f, 14, sf::Color(190, 235, 190), false);
    text("攻击 " + std::to_string(op->cs.atk) + "    防御 " + std::to_string(op->cs.def),
         44.f, 14, sf::Color(215, 215, 225), false);
    text(std::string("攻击类型 ") + (op->cs.magic ? "法术" : "物理"), 62.f, 14,
         op->cs.magic ? sf::Color(205, 150, 240) : sf::Color(240, 190, 140), false);
}

// 画出所有飞行中的弹体：箭矢随方向旋转、法球直接飞（缺图兜底小圆点）。
static void drawProjectiles(sf::RenderWindow& win, const Game& g)
{
    for (const Projectile& p : g.projectiles)
    {
        float k = (p.dur > 0.f) ? p.t / p.dur : 1.f;
        if (k > 1.f) k = 1.f;
        sf::Vector2f pos = p.from + (p.to - p.from) * k;

        if (!hasOpHit[p.opType])   // 没有该干员的 _hit 贴图 → 退化成一个职业色小圆点
        {
            sf::CircleShape dot(4.f);
            dot.setOrigin(sf::Vector2f{4.f, 4.f});
            dot.setPosition(pos);
            dot.setFillColor(operatorColor(p.opType));
            win.draw(dot);
            continue;
        }
        bool spin = OP_DEF[p.opType].spin;             // 该干员的弹道是否转向（箭矢转、法球不转）
        sf::Sprite spr(opHitTex[p.opType]);
        sf::Vector2u ts = opHitTex[p.opType].getSize();
        float target = spin ? CELL_W * 0.55f : CELL_W * 0.5f;
        float scale = target / float(std::max(ts.x, ts.y));
        spr.setScale(sf::Vector2f{scale, scale});
        spr.setOrigin(spr.getLocalBounds().size / 2.f);
        spr.setPosition(pos);
        if (spin)   // 箭矢型弹道：转向飞行方向
        {
            float ang = std::atan2(p.to.y - p.from.y, p.to.x - p.from.x) * 180.f / 3.14159265f;
            spr.setRotation(sf::degrees(ang + SNIPER_ARROW_BASE_DEG));
        }
        win.draw(spr);
    }
}

// 点了场上干员后，在它头顶画一个「撤退」按钮（escape 图标 + 白框，表示可点）。
static void drawRetreatButton(sf::RenderWindow& win, const Game& g)
{
    if (g.retreatRow < 0) return;
    sf::FloatRect r = retreatButtonRect(g.retreatRow, g.retreatCol);
    sf::Vector2f  c{ r.position.x + r.size.x / 2.f, r.position.y + r.size.y / 2.f };

    sf::RectangleShape frame(r.size);         // 白框底，突出「可点击」
    frame.setPosition(r.position);
    frame.setFillColor(sf::Color(20, 22, 28, 235));
    frame.setOutlineThickness(-2.f);
    frame.setOutlineColor(sf::Color(255, 255, 255, 220));
    win.draw(frame);

    if (hasEscapeTex)
    {
        sf::Sprite ic(escapeTex);
        sf::Vector2u ts = escapeTex.getSize();
        float scale = (r.size.x * 0.9f) / float(ts.x);
        ic.setScale(sf::Vector2f{scale, scale});
        ic.setOrigin(ic.getLocalBounds().size / 2.f);
        ic.setPosition(c);
        win.draw(ic);
    }
    else if (hasFont)
    {
        std::string s = "撤";
        sf::Text t(font, sf::String::fromUtf8(s.begin(), s.end()), 24);
        t.setStyle(sf::Text::Bold);
        t.setFillColor(sf::Color(235, 90, 90));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(b.position + b.size / 2.f);
        t.setPosition(c);
        win.draw(t);
    }
}

// 明日方舟式底部部署栏：一排干员头像卡（_fig 立绘），高亮选中的那张；右下角显示 DP。
static void drawDeployPanel(sf::RenderWindow& win, const Game& g)
{
    sf::RectangleShape bar(sf::Vector2f{float(BOARD_W + HUD_W), float(DEPLOY_H)});
    bar.setPosition(sf::Vector2f{0.f, float(BOARD_H)});
    bar.setFillColor(sf::Color(22, 26, 34));
    win.draw(bar);

    for (int op_id = 0; op_id < OP_COUNTS; op_id++)
    {
        sf::FloatRect r = deployCardRect(op_id);
        bool  sel     = (g.selectedId == op_id);
        bool  afford  = (g.dp >= Operator::costOf(op_id));
        bool  onField = g.isOpDeployed(op_id);        // 已经有一个在场
        float cd      = g.opCooldown[op_id];            // 再部署冷却剩余秒数
        bool  enabled = !onField && cd <= 0.f && afford;

        sf::RectangleShape card(r.size);
        card.setPosition(r.position);
        card.setFillColor(sf::Color(45, 52, 66));
        win.draw(card);

        if (hasOpAvatar[op_id])   // 头像（_avatar 256² 方图，直接等比填满方形卡片，各卡天然对齐）
        {
            sf::Sprite av(opAvatarTex[op_id]);
            sf::Vector2u ts = opAvatarTex[op_id].getSize();
            av.setScale(sf::Vector2f{ r.size.x / float(ts.x), r.size.y / float(ts.y) });
            av.setPosition(r.position);
            if (!enabled) av.setColor(sf::Color(110, 110, 110));  // 不可部署时压暗
            win.draw(av);
        }

        sf::RectangleShape accent(sf::Vector2f{r.size.x, 5.f});    // 顶部职业色
        accent.setPosition(r.position);
        accent.setFillColor(operatorColor(op_id));
        win.draw(accent);

        sf::RectangleShape frame(r.size);                          // 选中描边
        frame.setPosition(r.position);
        frame.setFillColor(sf::Color::Transparent);
        frame.setOutlineThickness(sel ? -3.f : -1.f);
        frame.setOutlineColor(sel ? sf::Color::White : sf::Color(90, 100, 120));
        win.draw(frame);

        // 已在场 / 冷却中：盖一层暗幕，再叠状态字（冷却=倒计时秒数，在场=「已部署」）
        if (onField || cd > 0.f)
        {
            sf::RectangleShape ov(r.size);
            ov.setPosition(r.position);
            ov.setFillColor(sf::Color(0, 0, 0, 120));
            win.draw(ov);
        }
        if (hasFont && cd > 0.f)
        {
            std::string s = std::to_string(int(std::ceil(cd)));
            sf::Text t(font, sf::String::fromUtf8(s.begin(), s.end()), 30);
            t.setStyle(sf::Text::Bold);
            t.setFillColor(sf::Color::White);
            sf::FloatRect b = t.getLocalBounds();
            t.setOrigin(b.position + b.size / 2.f);
            t.setPosition(sf::Vector2f{r.position.x + r.size.x / 2.f, r.position.y + r.size.y / 2.f});
            win.draw(t);
        }
        else if (hasFont && onField)
        {
            std::string s = "已部署";
            sf::Text t(font, sf::String::fromUtf8(s.begin(), s.end()), 14);
            t.setFillColor(sf::Color(190, 255, 190));
            sf::FloatRect b = t.getLocalBounds();
            t.setOrigin(b.position + b.size / 2.f);
            t.setPosition(sf::Vector2f{r.position.x + r.size.x / 2.f, r.position.y + r.size.y / 2.f});
            win.draw(t);
        }

        int role = OP_DEF[op_id].role;                              // 右上角职业图标（按干员职业派生，同职业同图标）
        if (hasClassTex[role])
        {
            const float box = 22.f;
            sf::Sprite ic(classTex[role]);
            sf::Vector2u ts = classTex[role].getSize();
            float sc = box / float(std::max(ts.x, ts.y));          // 保持比例缩进小方框
            ic.setScale(sf::Vector2f{sc, sc});
            ic.setPosition(sf::Vector2f{r.position.x + r.size.x - box - 3.f, r.position.y + 3.f});
            win.draw(ic);
        }

        if (hasFont)
        {
            sf::RectangleShape costBg(sf::Vector2f{28.f, 20.f});   // 左上角费用
            costBg.setPosition(sf::Vector2f{r.position.x + 2.f, r.position.y + 6.f});
            costBg.setFillColor(sf::Color(0, 0, 0, 150));
            win.draw(costBg);

            std::string cs = std::to_string(Operator::costOf(op_id));
            sf::Text tc(font, sf::String::fromUtf8(cs.begin(), cs.end()), 16);
            tc.setStyle(sf::Text::Bold);
            tc.setFillColor(afford ? sf::Color(255, 215, 80) : sf::Color(160, 160, 160));
            tc.setPosition(sf::Vector2f{r.position.x + 6.f, r.position.y + 5.f});
            win.draw(tc);

            sf::RectangleShape nameBg(sf::Vector2f{r.size.x, 20.f});  // 底部名字条
            nameBg.setPosition(sf::Vector2f{r.position.x, r.position.y + r.size.y - 20.f});
            nameBg.setFillColor(sf::Color(0, 0, 0, 150));
            win.draw(nameBg);

            std::string nm = Operator::nameOf(op_id);
            sf::Text tn(font, sf::String::fromUtf8(nm.begin(), nm.end()), 14);
            tn.setFillColor(sf::Color::White);
            tn.setPosition(sf::Vector2f{r.position.x + 6.f, r.position.y + r.size.y - 21.f});
            win.draw(tn);
        }
    }

    // 右下角：部署费用数字 + 费用图标（cost）
    float iconSize = 44.f;
    float iconX = float(BOARD_W + HUD_W) - 12.f - iconSize;
    float iconY = float(BOARD_H) + (float(DEPLOY_H) - iconSize) / 2.f;
    float midY  = float(BOARD_H) + float(DEPLOY_H) / 2.f;
    if (hasCostTex)
    {
        sf::Sprite ic(costTex);
        sf::Vector2u ts = costTex.getSize();
        ic.setScale(sf::Vector2f{iconSize / float(ts.x), iconSize / float(ts.y)});
        ic.setPosition(sf::Vector2f{iconX, iconY});
        win.draw(ic);
    }
    if (hasFont)
    {
        std::string s = std::to_string(g.dp);
        sf::Text t(font, sf::String::fromUtf8(s.begin(), s.end()), 30);
        t.setStyle(sf::Text::Bold);
        t.setFillColor(sf::Color(255, 215, 80));
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(sf::Vector2f{b.position.x + b.size.x, b.position.y + b.size.y / 2.f});  // 右-中对齐
        t.setPosition(sf::Vector2f{iconX - 8.f, midY});
        win.draw(t);
    }
}

// 游戏结束：半透明幕布 + 居中的「胜利/失败」提示。
static void drawGameOver(sf::RenderWindow& win, const Game& g)
{
    float W = float(BOARD_W + HUD_W), H = float(BOARD_H + DEPLOY_H);   // 逻辑尺寸（不用像素）
    sf::RectangleShape veil(sf::Vector2f{W, H});
    veil.setFillColor(sf::Color(0, 0, 0, 160));
    win.draw(veil);

    if (!hasFont) return;
    auto centered = [&](const std::string& s, unsigned size, sf::Color col, float cy) {
        sf::Text t(font, sf::String::fromUtf8(s.begin(), s.end()), size);
        t.setFillColor(col);
        t.setStyle(sf::Text::Bold);
        sf::FloatRect b = t.getLocalBounds();
        t.setOrigin(b.position + b.size / 2.f);
        t.setPosition(sf::Vector2f(W / 2.f, cy));
        win.draw(t);
    };
    float midY = H / 2.f;
    if (g.victory)
    {
        centered("游戏胜利！", 52, sf::Color(90, 220, 130), midY - 20.f);
        centered("敌人已清空", 22, sf::Color(210, 210, 210), midY + 30.f);
    }
    else
    {
        centered("游戏失败！", 52, sf::Color(235, 80, 80), midY - 20.f);
        centered("生命值归零", 22, sf::Color(210, 210, 210), midY + 30.f);
    }
}


// ============================================================
//  总入口：按图层顺序把整帧画出来（原 Game::draw）
// ============================================================
void render(sf::RenderWindow& win, const Game& g)
{
    drawMap(win);
    drawSelectedRange(win, g);                         // 被点选干员的攻击范围（淡，在单位之下）
    for (const Operator& o : g.operators) drawOperator(win, o);
    for (const Enemy&    e : g.enemies)   drawEnemy(win, e);
    drawProjectiles(win, g);                           // 飞行中的弹体（在单位之上、UI 之下）
    drawSelectedInfo(win, g);                          // 被点选干员的信息小卡（在单位之上）
    drawRetreatButton(win, g);                         // 撤退按钮浮在干员头顶
    drawTopStatus(win, g);                             // 正上方中间：进度 + 生命
    drawHud(win);
    drawDeployPanel(win, g);
    if (g.gameOver) drawGameOver(win, g);
}


// 让固定逻辑视图(view)在窗口里保持宽高比：多出来的边留黑边(letterbox)，而不是拉伸变形。
void applyLetterbox(sf::View& view, sf::Vector2u winSize)
{
    float windowRatio = float(winSize.x) / float(winSize.y);
    float viewRatio   = float(BOARD_W + HUD_W) / float(BOARD_H + DEPLOY_H);
    float sx = 1.f, sy = 1.f, px = 0.f, py = 0.f;
    if (windowRatio >= viewRatio) { sx = viewRatio / windowRatio; px = (1.f - sx) / 2.f; }  // 窗口偏宽→左右黑边
    else                          { sy = windowRatio / viewRatio; py = (1.f - sy) / 2.f; }  // 窗口偏高→上下黑边
    view.setViewport(sf::FloatRect({px, py}, {sx, sy}));
}
