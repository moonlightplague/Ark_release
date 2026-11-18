// ============================================================
//  game.cpp —— ⚙️ 后端·规则的「实现」·【M3 学生版（挖空，待你填）】
// ============================================================
//  ★ 标出的函数留给你实现，其余都是【已给】的引擎/工具。默认构建编译的就是本文件，
//  所以你每填好一个函数、重新编译运行，就能立刻看到游戏往前进一步。
//  完整答案在 game.cpp.answer（老师侧参考）——请先自己写，别直接抄。
//
//  五个任务（前四个在本文件，第 ⑤ 个在 game.h 的数据表；每做完一个都能编译、运行、看到新变化）：
//    ① 干员构造函数 Operator::Operator                    → 干员显示各自正确属性、落到点击的格
//    ② 索敌 operatorAt / inRange / findEnemyInRange        → 干员朝射程内敌人开火、黑角挡怪
//    ③ 攻击节奏 attackReady                                → 开火从「随机·平均 5 秒一发」变各干员按自己的间隔正常输出
//    ④ 挨打 takeDamage                                     → 敌人真的掉血、会死、防御生效
//    ⑤ 新增干员「艾雅法拉」（在 game.h 的 OP_DEF 数据表）  → 体会：加一行数据 + 改一个计数，就多一个干员
//  没实现的函数现在都是「占位实现」：能编译、不崩、但不是真逻辑，所以游戏每一步都能跑。
// ============================================================

#include "game.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>   // std::rand / RAND_MAX（任务③ 的随机占位用）


// 伤害公式（工具，声明在 game.h）：物理 = 攻击力 - 目标防御；法术 = 攻击力 ×(1 - 目标法抗)；都不低于攻击力的 5%。
int computeDamage(const CombatStats& a, const CombatStats& d)
{
    int base   = a.magic ? int(std::lround(a.atk * (1.0 - d.resist))) : (a.atk - d.def);
    int minDmg = int(std::ceil(0.05 * a.atk));
    return std::max(base, minDmg);
}


// ---------- Enemy ----------

// 构造：从 ENEMY_DEF[t] 取该种敌人的血量/移速/战斗属性来初始化（成员初始化列表）。
Enemy::Enemy(int r, int c, int t, int id_, float startDelay)
    : row(r), col(c), prevRow(r), prevCol(c), type(t), id(id_),
      hp(ENEMY_DEF[t].maxHp), maxHp(ENEMY_DEF[t].maxHp), cs(ENEMY_DEF[t].cs),
      atkTimer(cs.interval), atkAnim(0.f), alive(true), blocked(false),
      stepTime(ENEMY_DEF[t].stepTime), step(startDelay) {}

// 每格用时：spawnWaves 排错峰出场时按 type 查表（实例还没造出来）。
float Enemy::stepTimeOf(int t) { return ENEMY_DEF[t].stepTime; }

// 是否已进场（step<0 = 还在场外排队错峰出场）。
bool Enemy::onField() const { return step >= 0.f; }

// 移动一步。blocked=true（同格有干员挡住）时停在原地专心打、不前进。
// 走到终点 E 返回 true（这只漏进了蓝门）。
bool Enemy::update(float dt, bool blocked)
{
    step += dt;   // 攻击动画衰减已移到 attackReady()；update 只管移动
    if (step >= stepTime)
    {
        if (blocked) { step = stepTime; return false; }   // 被干员挡住：不走（战斗结算里去打它）
        step -= stepTime;
        prevRow = row;  prevCol = col;                 // 走之前记住上一格
        if (row == 1 && col == 0) { alive = false; return true; }   // 到终点 E → 漏了
        else if (col > 0        && (map[row][col-1] == '#' || map[row][col-1] == 'E')) --col;
        else if (row > 0        && (map[row-1][col] == '#' || map[row-1][col] == 'E')) --row;
        else if (row < GRID_H-1 && (map[row+1][col] == '#' || map[row+1][col] == 'E')) ++row;
        else if (col < GRID_W-1 && (map[row][col+1] == '#' || map[row][col+1] == 'E')) ++col;
    }
    return false;
}

// —— Enemy 的封装成员（把「对象对自己做的事」收进对象）——

// 血条比例 = 当前血量 / 最大血量（画血条用）。
float Enemy::hpRatio() const { return float(hp) / float(maxHp); }

// ★★★ M3 任务③ · 攻击节奏：Enemy::attackReady ★★★
// 目标：先衰减出手动画 atkAnim，再累加攻击计时器 atkTimer，攒够「自己的攻击间隔 cs.interval」就返回 true（进行一次攻击）并在攻击计时器中扣除攻击间隔（开始准备下次攻击）。
//   现在是【占位】：每帧以 dt/5 的概率随机决定出手 → 平均每 5 秒打一次（随机、没按 cs.interval，也没管 atkAnim）。
bool Enemy::attackReady(float dt)
{
    if (atkAnim > 0.f) { atkAnim -= dt; if (atkAnim < 0.f) atkAnim = 0.f; } //更新出手动画，无需修改
    // 待完成：修改为按照攻击间隔判断当前帧是否进行攻击
    return (float(std::rand()) / float(RAND_MAX)) < dt / 5.f;
}

// 出手 / 待机（配合 attackReady 用）【已给，不用改】
void Enemy::onAttack()   { atkTimer = 0.f; atkAnim = ATK_ANIM_TIME; }   // 出手了：计时器归零 + 触发贴图摇一下
void Enemy::holdAttack() { atkTimer = cs.interval; }                    // 没目标：保持就绪，一有目标立刻打

// ★★★ M3 任务④ · 挨打：Enemy::takeDamage ★★★
// 目标：用已给的工具 computeDamage(atk, cs) 算出实际伤害，从 hp 里扣掉；hp<=0 就把 alive 置 false（下场）。
//   现在是空的：敌人不掉血、打不死。
void Enemy::takeDamage(const CombatStats& atk)
{
    //待完成
}


// ---------- Operator ----------

// ★★★ M3 任务① · 干员构造函数 Operator::Operator ★★★
// 目标：用成员初始化列表，从 OP_DEF[op_id] 取这个干员的 血量/费用/名字/战斗属性/阻挡数 来初始化，落在r行c列位置。
//   现在是空构造函数：干员成员全走 game.h 里的默认值（落在 (0,0)、100 血、名字 "?"、射程 1）——
//   能编译能跑，但每个干员都长一样、落点也不对。你要把它写对。
//   ★参考上面【已给】的 Enemy 构造函数：它就是「: row(r), col(c), ... hp(ENEMY_DEF[t].maxHp), ... {}」的同款写法。
Operator::Operator(int r, int c, int op_id)
{
    //待完成
}



// 按干员编号 type 查数据（部署前还没有实例时用；都从 OP_DEF 取，不是按职业写死）。
int         Operator::costOf(int op_id) { return OP_DEF[op_id].cost; }   // 部署费用
const char* Operator::nameOf(int op_id) { return OP_DEF[op_id].name; }   // 干员名
int         Operator::role  (int op_id) { return OP_DEF[op_id].role; }   // 职业（OP_SNIPER/DEFENDER/CASTER）

// ★★★ M3 任务② · 索敌之二：Operator::inRange ★★★
// 目标：判断敌人 e 是否落在本干员攻击范围内。
//   现在直接返回 false：所以干员永远索不到目标、不会开火。
bool Operator::inRange(const Enemy& e) const
{
    //待完成：根据干员和敌人所在格子进行判定
    return false;
}

// 血条比例 = 当前血量 / 最大血量（画血条用）。【已给】
float Operator::hpRatio() const { return float(hp) / float(maxHp); }

// ★★★ M3 任务③ · 攻击节奏：Operator::attackReady ★★★
//（和 Enemy::attackReady 是同款——这份「两个类各写一遍」的重复）
// 目标：先衰减出手动画 atkAnim，再累加攻击计时器 atkTimer，攒够「自己的攻击间隔 cs.interval」就返回 true（进行一次攻击）并在攻击计时器中扣除攻击间隔（开始准备下次攻击）。
// 目标：atkAnim 衰减 + atkTimer 累加，攒够 cs.interval 返回 true。现在是【占位】随机·平均 5 秒一发。
bool Operator::attackReady(float dt)
{
    if (atkAnim > 0.f) { atkAnim -= dt; if (atkAnim < 0.f) atkAnim = 0.f; } //更新出手动画，无需修改
    // 待完成：修改为按照攻击间隔判断当前帧是否进行攻击
    return (float(std::rand()) / float(RAND_MAX)) < dt / 5.f;
}

// 出手 / 待机（配合 attackReady 用）【已给，不用改】
void Operator::onAttack()   { atkTimer = 0.f; atkAnim = ATK_ANIM_TIME; }   // 出手了：计时器归零 + 触发立绘摇一下
void Operator::holdAttack() { atkTimer = cs.interval; }                    // 没目标：保持就绪，一有目标立刻打

// ★★★ M3 任务④ · 挨打：Operator::takeDamage ★★★（和 Enemy 的几乎一样，只是干员没有 alive 字段）
// 目标：用 computeDamage(atk, cs) 算实伤，从 hp 里扣掉（干员的倒下由 cullDead 按 hp<=0 处理，这里不用管）。
//   现在是空的：干员不掉血。
void Operator::takeDamage(const CombatStats& atk)
{
    // 待完成
}


// ---------- Game：编排 ----------

// 安排有时间间隔的 2 波：第1波 3 源石虫；第2波 1 源石虫 + 2 士兵（共 6）。
// 复用「step<0 = 还没进场」的机制：越晚出场的敌人，初始 step 越负。同波内每个错开
// 「前一个的每格用时」（前一个走出 S 后下一个才进，不在 S 堆叠）；第2波整体再晚 WAVE_GAP。
void Game::spawnWaves()
{
    enemies.clear();
    int w1[] = { ENEMY_SLUG, ENEMY_SLUG, ENEMY_SLUG };          // 第1波
    int w2[] = { ENEMY_SLUG, ENEMY_SOLDIER, ENEMY_SOLDIER };    // 第2波
    float t = 0.f; int id = 0;
    for (int k : w1) { enemies.push_back(Enemy(2, 8, k, id++, -t)); t += Enemy::stepTimeOf(k); }
    t += WAVE_GAP;                                              // 两波之间留间隔
    for (int k : w2) { enemies.push_back(Enemy(2, 8, k, id++, -t)); t += Enemy::stepTimeOf(k); }
    remainingEnemies = int(enemies.size());                    // 共 6
    enemiesTotal     = int(enemies.size());                    // 记住总数
}

void Game::update(float dt)
{
    if (gameOver) return;

    dpTimer += dt;                                     // DP 每 dpInterval 秒回 1 点
    if (dpTimer >= dpInterval) { dpTimer -= dpInterval; ++dp; }

    for (int op_id = 0; op_id < OP_COUNTS; op_id++)     // 干员再部署冷却倒计时
        if (opCooldown[op_id] > 0.f) { opCooldown[op_id] -= dt; if (opCooldown[op_id] < 0.f) opCooldown[op_id] = 0.f; }

    // 1) 移动：同格有干员就可能被挡，但每个干员最多挡「阻挡数」个敌人，多出来的放行
    for (Operator& op : operators) op.blockUsed = 0;   // 每帧先把各干员的「已挡数」清零
    for (Enemy& e : enemies)
        if (e.alive)
        {
            Operator* op = e.onField() ? operatorAt(e.row, e.col) : nullptr;
            bool blocked = false;
            if (op && op->blockUsed < op->blockCount) { blocked = true; ++op->blockUsed; }   // 还有空位才挡
            e.blocked = blocked;                       // 存回：供绘制错开 + 战斗结算判定
            if (e.update(dt, blocked)) --life;         // 漏进蓝门 → 扣血
        }

    resolveCombat(dt);                                 // 2) 战斗结算
    cullDead();                                        // 3) 清理阵亡/漏掉的单位

    for (Projectile& p : projectiles) p.t += dt;       // 推进弹体，飞完就回收
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
                      [](const Projectile& p){ return p.t >= p.dur; }), projectiles.end());

    // 4) 剩余敌人 = 队列里还剩几个（含未进场的）；再判胜负
    remainingEnemies = int(enemies.size());
    if (life <= 0)                  { gameOver = true; victory = false; }
    else if (remainingEnemies <= 0) { gameOver = true; victory = true;  }
}

// 每帧战斗结算：干员打射程内第一个敌人；敌人打同格挡住自己的干员。
void Game::resolveCombat(float dt)
{
    for (Operator& op : operators)                     // —— 干员攻击 ——
    {
        if (op.attackReady(dt))
        {
            Enemy* target = findEnemyInRange(op);
            if (target)
            {
                target->takeDamage(op.cs);   // 结算伤害（血空自动置 alive=false）
                op.onAttack();               // 出手：计时器归零 + 触发摇晃

                // 远程干员(rangeRadius>0)开火 → 生成一枚飞向目标的弹体（纯视觉；伤害已在上面结算）
                // 弹体只记「谁发的」(op.id)，前端据此按 <asset>_hit 取贴图 / 职业色 / 是否旋转。
                if (op.cs.rangeRadius > 0)
                {
                    sf::Vector2f a = cellCenter(op.row, op.col);  a.y -= CELL_H * 0.2f;
                    sf::Vector2f b = cellCenter(target->row, target->col);
                    float dur = std::max(0.06f, std::hypot(b.x - a.x, b.y - a.y) / PROJECTILE_SPEED);
                    projectiles.push_back(Projectile{ a, b, 0.f, dur, op.id });
                }
            }
            else op.holdAttack();            // 没目标：保持就绪，一有目标立刻打
        }
    }

    for (Enemy& e : enemies)                           // —— 敌人攻击（只有被挡住的敌人才停下打干员）——
    {
        if (!e.alive || !e.onField()) continue;
        if (e.attackReady(dt))
        {
            Operator* target = e.blocked ? operatorAt(e.row, e.col) : nullptr;   // 没被挡住就不停手，继续走
            if (target) { target->takeDamage(e.cs); e.onAttack(); }
            else e.holdAttack();
        }
    }
}

// 清理：干员阵亡 → 进再部署冷却并移除；敌人漏掉/被击杀 → 移除。
void Game::cullDead()
{
    for (size_t i = 0; i < operators.size(); )
    {
        if (operators[i].hp <= 0)
        {
            opCooldown[operators[i].id] = DEPLOY_COOLDOWN;   // 复用「再部署冷却」
            if (retreatRow == operators[i].row && retreatCol == operators[i].col)
                retreatRow = retreatCol = -1;                  // 正弹着它的撤退按钮就收起
            operators.erase(operators.begin() + i);
        }
        else i++;
    }
    for (size_t i = 0; i < enemies.size(); )
    {
        if (!enemies[i].alive) enemies.erase(enemies.begin() + i);   // 漏掉(update置false)或被击杀
        else i++;
    }
}

// ★★★ M3 任务② · 索敌之一：operatorAt ★★★
// 目标：返回站在 (row,col) 这一格上的干员指针；没有就返回 nullptr。（提示：遍历 operators。）
//   现在直接返回 nullptr：所以「重装干员挡怪」「敌人打同格干员」都还不生效。
Operator* Game::operatorAt(int row, int col)
{

    // 待完成
    return nullptr;
}

// ★★★ M3 任务② · 索敌之三：findEnemyInRange ★★★
// 目标：返回列表里第一个「活着·已进场·血量>0·在 op 射程内」的敌人（提示：用上面写好的 op.inRange(e) 判断）；没有则 nullptr。
//   现在直接返回 nullptr：所以干员没有目标、不开火。
Enemy* Game::findEnemyInRange(const Operator& op)
{
    // 待完成
    return nullptr;
}

// 当前被点选（正显示信息卡/撤退按钮）的干员；没有则 nullptr。
const Operator* Game::selectedOperator() const
{
    if (retreatRow < 0) return nullptr;
    for (const Operator& o : operators)
        if (o.row == retreatRow && o.col == retreatCol) return &o;
    return nullptr;
}

// 鼠标左键（mx,my 已是逻辑坐标）：撤退按钮 / 干员卡 / 棋盘，分情况处理。
void Game::handleClick(float mx, float my)
{
    if (gameOver) return;

    // 1) 撤退按钮正弹着，且点在按钮上 → 确认撤退（移走干员 + 进入再部署冷却）
    if (retreatRow >= 0 &&
        retreatButtonRect(retreatRow, retreatCol).contains(sf::Vector2f{mx, my}))
    {
        retreatAt(retreatRow, retreatCol);
        retreatRow = retreatCol = -1;                    // 收起按钮
        return;
    }

    // 2) 点干员卡 → 选中（顺便收起撤退按钮）
    for (int op_id = 0; op_id < OP_COUNTS; op_id++)
        if (deployCardRect(op_id).contains(sf::Vector2f{mx, my}))
        {
            selectedId = op_id;
            retreatRow = retreatCol = -1;
            return;
        }

    // 3) 点棋盘
    if (mx < BOARD_W && my < BOARD_H)
    {
        int col = int(mx / CELL_W);
        int row = int(my / CELL_H);

        // 3a) 点到已部署的干员 → 切换它头顶的撤退按钮（再点同一个 = 收起）
        if (isCellOccupied(row, col))
        {
            if (retreatRow == row && retreatCol == col)
                retreatRow = retreatCol = -1;
            else { retreatRow = row; retreatCol = col; }
            selectedId = -1;                           // 撤退与部署互斥
            return;
        }

        // 3b) 点空格子：收起撤退按钮；若选了干员卡就尝试部署
        retreatRow = retreatCol = -1;
        if (selectedId >= 0 && tryPlaceOperator(row, col, selectedId))
            selectedId = -1;                           // 部署成功 → 取消选中
    }
}

// 放置规则：由干员【职业】决定——重装(OP_DEFENDER)近战→地面 '#'；其余(狙击/术师)远程→高台 'P'。
// （数据驱动：加干员只在 OP_DEF 那行写好职业，这里不用改。）
bool Game::canPlaceHere(int row, int col, int op_id) const
{
    if (row < 0 || row >= GRID_H || col < 0 || col >= GRID_W) return false;
    char t = map[row][col];
    return OP_DEF[op_id].role == OP_DEFENDER ? (t == '#') : (t == 'P');
}
bool Game::isCellOccupied(int row, int col) const
{
    for (const Operator& o : operators)
        if (o.row == row && o.col == col) return true;
    return false;
}
bool Game::isOpDeployed(int op_id) const                 // 干员是不是已经在场上了
{
    for (const Operator& o : operators)
        if (o.id == op_id) return true;
    return false;
}
bool Game::tryPlaceOperator(int row, int col, int op_id)
{
    if (!canPlaceHere(row, col, op_id)) return false;   // 地形不对
    if (isCellOccupied(row, col))       return false;   // 一格只能一个干员
    if (isOpDeployed(op_id))          return false;   // 干员已经在场
    if (opCooldown[op_id] > 0.f)        return false;   // 还在再部署冷却中
    if (dp < Operator::costOf(op_id))   return false;   // DP 不够
    operators.push_back(Operator(row, col, op_id));
    dp -= Operator::costOf(op_id);
    return true;
}
// 撤退：格子上有干员就移走，并让该种干员进入 DEPLOY_COOLDOWN 秒再部署冷却。
bool Game::retreatAt(int row, int col)
{
    for (size_t i = 0; i < operators.size(); i++)
        if (operators[i].row == row && operators[i].col == col)
        {
            opCooldown[operators[i].id] = DEPLOY_COOLDOWN;
            operators.erase(operators.begin() + i);
            return true;
        }
    return false;
}
