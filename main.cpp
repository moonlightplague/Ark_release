/*
 * ============================================================
 *  塔防课程项目  【SFML 版 · SFML 3.x】
 *  主题: 《明日方舟》风格 2D 塔防   —— M3 → 拆分重构版
 * ============================================================
 *
 *  M3 把全局变量 + 自由函数打包成了 Enemy / Operator / Game 三个类。
 *  进 M5「继承」之前，先把原来的单文件 main.cpp 拆成 4 个模块，
 *  按「这段代码在干嘛」分，而不是按软件工程术语分：
 *
 *    config.h          🎛️  可调旋钮：所有数字常量 + cellCenter
 *    game.h / game.cpp ⚙️  后端·规则：血量/伤害/索敌/移动/部署/胜负（不画任何东西）
 *    render.h/render.cpp 🎨 前端·美术：所有贴图 + 所有绘制（只有它碰贴图）
 *    main.cpp          🔌  总控（本文件）：开窗口、跑主循环、把输入交给后端
 *
 *  依赖是单向的：render 认识 game，game 不认识 render（模型不依赖视图）。
 *  因为所有贴图都私有在 render.cpp 里，全程【不需要一个 extern】。
 *
 *  拆分前的单文件版留档：main.cpp.m3（过程式历史版本：main.cpp.m1/.m2）。
 *  后续 M5：抽出 Unit 基类，Enemy/Operator 派生；再给干员加技能(Skill 多态)。
 * ============================================================
 */

#include "render.h"    // 前端接口（顺带带出 game.h → config.h，一行齐活）
#include <optional>

int main()
{
    sf::RenderWindow window(sf::VideoMode({unsigned(BOARD_W + HUD_W), unsigned(BOARD_H + DEPLOY_H)}),
                            "Arknights");
    window.setFramerateLimit(60);

    // 固定逻辑视图：无论窗口怎么缩放，游戏坐标始终是 (BOARD_W+HUD_W) × (BOARD_H+DEPLOY_H)。
    sf::View view(sf::FloatRect({0.f, 0.f}, {float(BOARD_W + HUD_W), float(BOARD_H + DEPLOY_H)}));
    applyLetterbox(view, window.getSize());
    window.setView(view);

    loadAssets();      // 前端：一次性载入字体 + 美术

    // 本局关卡参数在 config.h 的 GameConfig（要调初始费用/回费/生命改那里）。
    Game game;    // 后端：按 config.h 的 GameConfig 默认值开局（构造时就排好各波怪）
    sf::Clock clock;

    while (window.isOpen())
    {
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* rs = event->getIf<sf::Event::Resized>())   // 窗口缩放：重算 letterbox
            {
                applyLetterbox(view, rs->size);
                window.setView(view);
            }

            if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
                if (mb->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2f w = window.mapPixelToCoords(mb->position, view);  // 像素→逻辑坐标
                    game.handleClick(w.x, w.y);                                    // 交给后端处理
                }
        }

        game.update(clock.restart().asSeconds());   // 后端：推进一帧
        window.clear(sf::Color(0, 0, 40));
        window.setView(view);          // 每帧固定视图（保险）
        render(window, game);          // 前端：把这一帧画出来
        window.display();
    }
    return 0;
}
