#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <cmath>
#include <iostream>
#include <vector>

using namespace std;
using namespace sf;

const float PI = 3.14159265f;

// --- Helpers ---
Color lerpCol(Color a, Color b, float t) {
  t = max(0.f, min(1.f, t));
  return Color(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
               a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
}

float angLerp(float s, float e, float t) {
  float d = fmod(e - s, 360.f);
  if (d < 0.f)
    d += 360.f;
  return s + d * t;
}

ConvexShape buildRoundedRect(float w, float h, float r) {
  ConvexShape s(40);
  for (int i = 0; i < 10; ++i) {
    float a = i * PI / 18.f;
    s.setPoint(i, {w - r + r * cos(a), h - r + r * sin(a)});
  }
  for (int i = 0; i < 10; ++i) {
    float a = PI / 2 + i * PI / 18.f;
    s.setPoint(10 + i, {r + r * cos(a), h - r + r * sin(a)});
  }
  for (int i = 0; i < 10; ++i) {
    float a = PI + i * PI / 18.f;
    s.setPoint(20 + i, {r + r * cos(a), r + r * sin(a)});
  }
  for (int i = 0; i < 10; ++i) {
    float a = 3 * PI / 2 + i * PI / 18.f;
    s.setPoint(30 + i, {w - r + r * cos(a), r + r * sin(a)});
  }
  return s;
}

// --- UI Element (Combines Button and TextBox) ---
struct UIElement {
  RectangleShape rect;
  Text text;
  bool isInput;
  bool isSel = false;
  string val;

  UIElement() {}
  UIElement(float x, float y, float w, float h, string t, Font &f,
            bool input = false)
      : isInput(input), val(t) {
    rect.setPosition(x, y);
    rect.setSize({w, h});
    rect.setOutlineThickness(2);
    text.setFont(f);
    text.setCharacterSize(24);
    text.setFillColor(Color::White);
    updText();
  }

  void updText() {
    text.setString(val);
    FloatRect b = text.getLocalBounds();
    text.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
    text.setPosition(rect.getPosition().x + rect.getSize().x / 2.f,
                     rect.getPosition().y + rect.getSize().y / 2.f);
  }

  void handle(Event &e) {
    if (isInput && isSel && e.type == Event::TextEntered) {
      if (e.text.unicode == '\b' && !val.empty())
        val.pop_back();
      else if (e.text.unicode >= '0' && e.text.unicode <= '9' && val.size() < 5)
        val += (char)e.text.unicode;
      updText();
    }
  }

  bool update(Vector2f mPos) {
    bool hov = rect.getGlobalBounds().contains(mPos);
    rect.setFillColor(Color(isInput ? 30 : (hov ? 80 : 50),
                            isInput ? 30 : (hov ? 80 : 50),
                            isInput ? 30 : (hov ? 80 : 50), 200));
    rect.setOutlineColor(isSel ? Color::Yellow
                               : (hov ? Color(200, 200, 200) : Color::White));
    return hov;
  }

  void draw(RenderWindow &w) {
    w.draw(rect);
    w.draw(text);
  }
};

struct Person {
  int id;
  CircleShape shape;
  Text idText;
  bool alive = true;
  float angle, curR, tgtR;
  Color cOut, tOut, cFill, tFill, cTxt, tTxt;
};

struct SmokeParticle {
  CircleShape shape;
  Vector2f vel;
  float life, maxLife;
  SmokeParticle(Vector2f pos, Vector2f v, float l)
      : vel(v), life(l), maxLife(l) {
    shape.setRadius(10.f);
    shape.setOrigin(10.f, 10.f);
    shape.setPosition(pos);
    shape.setFillColor(Color(255, 255, 255, 200));
  }
  bool update(float dt) {
    shape.move(vel * dt);
    shape.setRadius(shape.getRadius() + 10.f * dt);
    shape.setOrigin(shape.getRadius(), shape.getRadius());
    life -= dt;
    float alpha = max(0.f, (life / maxLife) * 200.f);
    Color c = shape.getFillColor();
    c.a = (Uint8)alpha;
    shape.setFillColor(c);
    return life > 0;
  }
};

enum State { Menu, Sim };
enum SimState { Ready, Step, WaitElim, AnimElim, AnimRet, Over };

// --- Main App ---
class JosephusApp {
  RenderWindow win;
  int n = 10, k = 3, start = 1, curIdx = 0, step = 0, aliveCnt = 0,
      lastElim = -1;
  Music bgM, eliM, winM, horseM;
  Texture bgT, horseNormalT, horseAggroT, flagT;
  Sprite bgS, horseS;
  vector<Sprite> flags;
  bool hasBg = false, hasHorse = false, hasFlag = false;
  Font font, tFont;
  vector<SmokeParticle> smokes;

  Text lblN, lblK, lblS, title, hud, vicTxt;
  UIElement bNm, bNp, bKm, bKp, bSm, bSp, btnStart, tbN, tbK, tbS;

  State state = Menu;
  SimState sState = Ready;
  vector<Person> ppl;
  Vector2f ctr = {950, 600};
  float mRad = 450.f, oRad = 550.f;
  ConvexShape hudBg, arrow;
  float curRot = 0, tgtRot = 0, curArrL = 450.f;
  string lastHud;
  Clock t, appClock;
  float animP = 0.f;
  float horseShowTime = 0.f;

  void updArrow(float l) {
    arrow.setPointCount(7);
    arrow.setPoint(0, {0, -6});
    arrow.setPoint(1, {l - 70, -6});
    arrow.setPoint(2, {l - 70, -18});
    arrow.setPoint(3, {l - 30, 0});
    arrow.setPoint(4, {l - 70, 18});
    arrow.setPoint(5, {l - 70, 6});
    arrow.setPoint(6, {0, 6});
  }

  void setup() {
    ppl.clear();
    aliveCnt = n;
    step = 0;
    lastElim = -1;
    curIdx = (start - 1) % n;
    float r = min(35.f, (2 * PI * mRad * 0.7f) / (2.f * n));

    for (int i = 0; i < n; i++) {
      Person p;
      p.id = i + 1;
      p.angle = i * 2 * PI / n - PI / 2;
      p.curR = p.tgtR = mRad;
      p.shape.setRadius(r);
      p.shape.setOrigin(r, r);
      p.shape.setOutlineThickness(4);
      p.cFill = p.tFill = Color(0, 0, 0, 150);
      p.cOut = p.tOut = (p.id == start) ? Color::Yellow : Color::Green;
      p.cTxt = p.tTxt = Color::White;

      p.idText.setFont(font);
      p.idText.setString(to_string(p.id));
      p.idText.setCharacterSize(r * 1.1f);
      FloatRect b = p.idText.getLocalBounds();
      p.idText.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
      ppl.push_back(p);
    }
    tgtRot = curRot = curIdx * 360.f / n - 90.f;
    arrow.setRotation(curRot);
    updArrow(curArrL = mRad);
    sState = Ready;
  }

public:
  JosephusApp() : win(VideoMode(1900, 1200), "Josephus") {
    win.setFramerateLimit(60);
    font.loadFromFile("C:/Windows/Fonts/arial.ttf");
    tFont.loadFromFile("C:/Windows/Fonts/impact.ttf");
    if (bgT.loadFromFile("bg_war.png")) {
      bgS.setTexture(bgT);
      bgS.setScale(1900.f / bgT.getSize().x, 1200.f / bgT.getSize().y);
      hasBg = true;
    }
    Image hN, hA;
    if (hN.loadFromFile("horse_normal.png") &&
        hA.loadFromFile("horse_aggro.png")) {
      Color bgN = hN.getPixel(0, 0), bgA = hA.getPixel(0, 0);
      for (unsigned int x = 0; x < hN.getSize().x; ++x)
        for (unsigned int y = 0; y < hN.getSize().y; ++y) {
          Color c = hN.getPixel(x, y);
          if (std::abs((int)c.r - (int)bgN.r) +
                  std::abs((int)c.g - (int)bgN.g) +
                  std::abs((int)c.b - (int)bgN.b) <
              60)
            hN.setPixel(x, y, Color(0, 0, 0, 0));
        }
      for (unsigned int x = 0; x < hA.getSize().x; ++x)
        for (unsigned int y = 0; y < hA.getSize().y; ++y) {
          Color c = hA.getPixel(x, y);
          if (std::abs((int)c.r - (int)bgA.r) +
                  std::abs((int)c.g - (int)bgA.g) +
                  std::abs((int)c.b - (int)bgA.b) <
              60)
            hA.setPixel(x, y, Color(0, 0, 0, 0));
        }
      horseNormalT.loadFromImage(hN);
      horseAggroT.loadFromImage(hA);
      horseS.setTexture(horseNormalT);
      horseS.setOrigin(horseNormalT.getSize().x, horseNormalT.getSize().y);
      horseS.setPosition(1900, 1200);
      horseS.setScale(0.3f, 0.3f);
      hasHorse = true;
    }
    Image fImg;
    if (fImg.loadFromFile("flag.png")) {
      Color bgF = fImg.getPixel(0, 0);
      for (unsigned int x = 0; x < fImg.getSize().x; ++x)
        for (unsigned int y = 0; y < fImg.getSize().y; ++y) {
          Color c = fImg.getPixel(x, y);
          if (std::abs((int)c.r - (int)bgF.r) +
                  std::abs((int)c.g - (int)bgF.g) +
                  std::abs((int)c.b - (int)bgF.b) <
              60)
            fImg.setPixel(x, y, Color(0, 0, 0, 0));
        }
      flagT.loadFromImage(fImg);
      for (int i = 0; i < 8; i++) {
        Sprite fs(flagT);
        fs.setOrigin(flagT.getSize().x / 2.f, flagT.getSize().y / 2.f);
        fs.setScale(0.15f, 0.15f);
        float a = i * 2 * PI / 8.f;
        fs.setPosition(950 + 120 * cos(a), 600 + 120 * sin(a));
        fs.setRotation(a * 180.f / PI + 90.f);
        flags.push_back(fs);
      }
      hasFlag = true;
    }
    bgM.openFromFile("nhac-xo-so.wav");
    bgM.setLoop(true);
    bgM.setVolume(50);
    bgM.play();
    eliM.openFromFile("eli.wav");
    winM.openFromFile("win.wav");
    horseM.openFromFile("horse.wav");

    lblN = Text("Soldiers (N):", font, 30);
    FloatRect bN = lblN.getLocalBounds();
    lblN.setOrigin(bN.left + bN.width / 2.f, bN.top + bN.height / 2.f);
    lblN.setPosition(950, 350);
    bNm = UIElement(850, 400, 50, 50, "-", font);
    tbN = UIElement(910, 400, 80, 50, "10", font, true);
    bNp = UIElement(1000, 400, 50, 50, "+", font);

    lblK = Text("Step (K):", font, 30);
    FloatRect bK = lblK.getLocalBounds();
    lblK.setOrigin(bK.left + bK.width / 2.f, bK.top + bK.height / 2.f);
    lblK.setPosition(950, 480);
    bKm = UIElement(850, 530, 50, 50, "-", font);
    tbK = UIElement(910, 530, 80, 50, "3", font, true);
    bKp = UIElement(1000, 530, 50, 50, "+", font);

    lblS = Text("Start Pos:", font, 30);
    FloatRect bS = lblS.getLocalBounds();
    lblS.setOrigin(bS.left + bS.width / 2.f, bS.top + bS.height / 2.f);
    lblS.setPosition(950, 610);
    bSm = UIElement(850, 660, 50, 50, "-", font);
    tbS = UIElement(910, 660, 80, 50, "1", font, true);
    bSp = UIElement(1000, 660, 50, 50, "+", font);

    btnStart = UIElement(850, 750, 200, 60, "START", font);

    title = Text("JOSEPHUS SIMULATION", tFont, 90);
    title.setFillColor(Color::Yellow);
    FloatRect tB = title.getLocalBounds();
    title.setOrigin(tB.left + tB.width / 2.f, tB.top + tB.height / 2.f);
    title.setPosition(950, 150);

    hudBg = buildRoundedRect(350, 180, 20);
    hudBg.setFillColor(Color(0, 0, 0, 100));
    hudBg.setOutlineThickness(2);
    hudBg.setPosition(20, 20);
    hud = Text("", font, 24);
    vicTxt = Text("", font, 40);
    vicTxt.setFillColor(Color::Yellow);
    arrow.setFillColor(Color::Cyan);
    arrow.setPosition(ctr);
  }

  void run() {
    while (win.isOpen()) {
      Vector2f mPos = win.mapPixelToCoords(Mouse::getPosition(win));
      Event e;
      while (win.pollEvent(e)) {
        if (e.type == Event::Closed)
          win.close();
        if (state == Menu) {
          tbN.handle(e);
          tbK.handle(e);
          tbS.handle(e);
          if (e.type == Event::MouseButtonPressed) {
            tbN.isSel = tbN.rect.getGlobalBounds().contains(mPos);
            tbK.isSel = tbK.rect.getGlobalBounds().contains(mPos);
            tbS.isSel = tbS.rect.getGlobalBounds().contains(mPos);

            n = stoi(tbN.val.empty() ? "1" : tbN.val);
            k = stoi(tbK.val.empty() ? "1" : tbK.val);
            start = stoi(tbS.val.empty() ? "1" : tbS.val);

            if (bNp.update(mPos))
              n++;
            if (bNm.update(mPos) && n > 2)
              n--;
            if (bKp.update(mPos))
              k++;
            if (bKm.update(mPos) && k > 1)
              k--;
            if (bSp.update(mPos))
              start++;
            if (bSm.update(mPos) && start > 1)
              start--;

            start = min(start, n);
            tbN.val = to_string(n);
            tbK.val = to_string(k);
            tbS.val = to_string(start);
            tbN.updText();
            tbK.updText();
            tbS.updText();

            if (btnStart.update(mPos)) {
              n = max(2, n);
              k = max(1, k);
              start = max(1, min(start, n));
              setup();
              state = Sim;
            }
          }
        } else if (e.type == Event::KeyPressed && sState == Ready) {
          sState = Step;
          t.restart();
        }
      }

      // Logic
      if (state == Sim) {
        if (sState == Step && t.getElapsedTime().asSeconds() > 0.8f) {
          t.restart();
          step++;
          tgtRot = curIdx * 360.f / n - 90.f;
          if (step == k)
            sState = WaitElim;
          else
            do {
              curIdx = (curIdx + 1) % n;
            } while (!ppl[curIdx].alive);
        } else if (sState == WaitElim &&
                   t.getElapsedTime().asSeconds() > 1.2f) {
          t.restart();
          sState = AnimElim;
          animP = 0.f;
          ppl[curIdx].alive = false;
          ppl[curIdx].tgtR = oRad;
          ppl[curIdx].tOut = Color(255, 50, 50);
          ppl[curIdx].tFill = Color(80, 0, 0, 150);
          ppl[curIdx].tTxt = Color(150, 150, 150);
          eliM.play();
          horseM.play();
          horseShowTime = 1.5f;
          if (hasHorse) {
            Vector2f snout(1900 - horseS.getGlobalBounds().width * 0.8f,
                           1200 - horseS.getGlobalBounds().height * 0.7f);
            for (int i = 0; i < 15; i++) {
              float vx = -200.f - (rand() % 100);
              float vy = -20.f + (rand() % 40);
              float life = 0.8f + (rand() % 100) / 100.f;
              smokes.push_back(SmokeParticle(snout, {vx, vy}, life));
            }
          }
        } else if (sState == AnimElim) {
          animP = min(1.f, animP + 0.02f);
          if (animP == 1.f && t.getElapsedTime().asSeconds() > 2.0f) {
            sState = AnimRet;
            animP = 0.f;
            lastElim = ppl[curIdx].id;
            step = 0;
            aliveCnt--;
            t.restart();
          }
        } else if (sState == AnimRet) {
          animP = min(1.f, animP + 0.02f);
          if (animP == 1.f && t.getElapsedTime().asSeconds() > 1.0f) {
            if (aliveCnt == 1) {
              for (int i = 0; i < n; i++)
                if (ppl[i].alive)
                  curIdx = i;
              tgtRot = curIdx * 360.f / n - 90.f;
              sState = Over;
              bgM.pause();
              winM.play();
            } else {
              do {
                curIdx = (curIdx + 1) % n;
              } while (!ppl[curIdx].alive);
              tgtRot = curIdx * 360.f / n - 90.f;
              sState = Step;
              t.restart();
            }
          }
        }

        // Animate
        if (abs(curRot - tgtRot) > 0.1f)
          arrow.setRotation(curRot = angLerp(curRot, tgtRot, 0.05f));
        float tLen = (sState == AnimElim) ? oRad : mRad;
        if (abs(curArrL - tLen) > 0.1f)
          updArrow(curArrL += (tLen - curArrL) * 0.05f);

        for (auto &p : ppl) {
          if (abs(p.curR - p.tgtR) > 0.1f)
            p.curR += (p.tgtR - p.curR) * 0.1f;
          p.shape.setPosition(ctr.x + p.curR * cos(p.angle),
                              ctr.y + p.curR * sin(p.angle));
          p.idText.setPosition(p.shape.getPosition());
          p.shape.setOutlineColor(p.cOut = lerpCol(p.cOut, p.tOut, 0.1f));
          p.shape.setFillColor(p.cFill = lerpCol(p.cFill, p.tFill, 0.1f));
          p.idText.setFillColor(p.cTxt = lerpCol(p.cTxt, p.tTxt, 0.1f));
        }

        for (auto it = smokes.begin(); it != smokes.end();) {
          if (!it->update(1.f / 60.f))
            it = smokes.erase(it);
          else
            ++it;
        }
        if (horseShowTime > 0.f)
          horseShowTime -= 1.f / 60.f;

        string st = (sState == Ready)
                        ? "READY"
                        : (sState == Over ? "COMPLETE!" : "RUNNING...");
        string s = "Status: " + st + "\nRemaining: " + to_string(aliveCnt) +
                   " / " + to_string(n) + "\nStep: " + to_string(step) + " / " +
                   to_string(k) + "\nLast Eliminated: " +
                   (lastElim == -1 ? "None" : to_string(lastElim));
        if (lastHud != s) {
          hud.setString(s);
          FloatRect b = hud.getLocalBounds();
          hud.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
          hud.setPosition(195, 110);
          lastHud = s;
        }
      }

      // Draw
      win.clear({30, 30, 30});
      if (hasBg)
        win.draw(bgS);
      if (state == Menu) {
        RectangleShape ov({1900, 1200});
        ov.setFillColor({0, 0, 0, 180});
        win.draw(ov);
        bNm.update(mPos);
        bNp.update(mPos);
        tbN.update(mPos);
        bKm.update(mPos);
        bKp.update(mPos);
        tbK.update(mPos);
        bSm.update(mPos);
        bSp.update(mPos);
        tbS.update(mPos);
        btnStart.update(mPos);

        win.draw(title);
        win.draw(lblN);
        bNm.draw(win);
        tbN.draw(win);
        bNp.draw(win);
        win.draw(lblK);
        bKm.draw(win);
        tbK.draw(win);
        bKp.draw(win);
        win.draw(lblS);
        bSm.draw(win);
        tbS.draw(win);
        bSp.draw(win);
        btnStart.draw(win);
      } else {
        CircleShape cp(mRad + 15);
        cp.setOrigin(mRad + 15, mRad + 15);
        cp.setPosition(ctr);
        cp.setFillColor({0, 0, 0, 100});
        cp.setOutlineThickness(3);
        win.draw(cp);
        for (auto &p : ppl) {
          win.draw(p.shape);
          win.draw(p.idText);
        }
        win.draw(arrow);
        if (hasFlag) {
          float gRot = appClock.getElapsedTime().asSeconds() * 20.f;
          for (int i = 0; i < 8; i++) {
            auto &f = flags[i];
            float a = (i * 360.f / 8.f + gRot) * PI / 180.f;
            f.setPosition(950 + 120 * cos(a), 600 + 120 * sin(a));
            f.setRotation(a * 180.f / PI + 90.f);
            win.draw(f);
          }
        }
        if (hasHorse) {
          if (horseShowTime > 0.f) {
            horseS.setTexture(horseAggroT);
            float p = horseShowTime / 1.5f;
            float s = 0.3f + 0.2f * p;
            horseS.setScale(s, s);
          } else {
            horseS.setTexture(horseNormalT);
            horseS.setScale(0.3f, 0.3f);
          }
          win.draw(horseS);
        }
        for (auto &s : smokes)
          win.draw(s.shape);
        win.draw(hudBg);
        win.draw(hud);

        if (sState == Over) {
          vicTxt.setString("VICTORY: SOLDIER " + to_string(ppl[curIdx].id) +
                           " IS THE WINNER!");
          FloatRect vr = vicTxt.getLocalBounds();
          vicTxt.setOrigin(vr.left + vr.width / 2.f, vr.top + vr.height / 2.f);
          vicTxt.setPosition(ctr.x, ctr.y + mRad / 2 + 50);
          RectangleShape wb({vr.width + 40, vr.height + 40});
          wb.setOrigin(wb.getSize().x / 2.f, wb.getSize().y / 2.f);
          wb.setPosition(vicTxt.getPosition());
          wb.setFillColor({0, 0, 0, 180});
          wb.setOutlineThickness(2);
          wb.setOutlineColor(Color::Yellow);
          win.draw(wb);
          win.draw(vicTxt);
        }
      }
      win.display();
    }
  }
};

int main() {
  JosephusApp().run();
  return 0;
}