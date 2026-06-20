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
  for (int i = 0; i < 40; ++i) {
    float a = i * PI / 18.f, cx = (i < 10 || i >= 30) ? w - r : r,
          cy = i < 20 ? h - r : r;
    s.setPoint(i, {cx + r * cos(a), cy + r * sin(a)});
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
  float curScale = 1.f, tgtScale = 1.f;
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

class ArrowShape : public sf::Drawable, public sf::Transformable {
  sf::VertexArray verts;
  sf::Color color;

  void addQuad(sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3,
               sf::Vector2f p4) {
    verts.append(sf::Vertex(p1, color));
    verts.append(sf::Vertex(p2, color));
    verts.append(sf::Vertex(p3, color));
    verts.append(sf::Vertex(p1, color));
    verts.append(sf::Vertex(p3, color));
    verts.append(sf::Vertex(p4, color));
  }

public:
  ArrowShape() : verts(sf::Triangles), color(sf::Color::White) {}

  void setFillColor(sf::Color c) {
    color = c;
    for (std::size_t i = 0; i < verts.getVertexCount(); ++i)
      verts[i].color = c;
  }

  void upd(float l) {
    verts.clear();
    float s = l / 450.f;
    float bw = 5.f * s;
    float W = 2.f * bw;

    float a1 = 66.f * s;
    float a2 = 21.6f * s;
    float b = 28.8f * s;
    float cx = l - a1 - 10.f * s;

    Vector2f Ro(cx + a1, 0), Lo(cx - a2, 0), To(cx, -b), Bo(cx, b);

    float L1 = sqrt(a1 * a1 + b * b);
    float L2 = sqrt(a2 * a2 + b * b);

    float ti_y = W * (L1 + L2) / (a1 + a2) - b;
    float ti_x = (W / b) * (L2 * a1 - L1 * a2) / (a1 + a2);

    Vector2f Ti(cx + ti_x, ti_y);
    Vector2f Bi(cx + ti_x, -ti_y);

    float ri_x = a1 - W * L1 / b;
    Vector2f Ri(cx + ri_x, 0);

    float li_x = -a2 + W * L2 / b;
    Vector2f Li(cx + li_x, 0);

    float x_end = cx - a2 + a2 * (bw / b);
    addQuad({0, -bw}, {x_end, -bw}, {x_end, bw}, {0, bw});

    addQuad(Ro, To, Ti, Ri);
    addQuad(To, Lo, Li, Ti);
    addQuad(Lo, Bo, Bi, Li);
    addQuad(Bo, Ro, Ri, Bi);
  }

  virtual void draw(sf::RenderTarget &target,
                    sf::RenderStates states) const override {
    states.transform *= getTransform();
    target.draw(verts, states);
  }
};

enum State { Menu, Sim };
enum SimState { Ready, Step, WaitElim, AnimElim, AnimRet, Over };

struct SpeakerIcon {
  Vector2f pos;
  float size = 40.f;
  bool isMuted = false;
  bool hov = false;

  SpeakerIcon(Vector2f p = {0, 0}) : pos(p) {}

  bool update(Vector2f mPos) {
    FloatRect bounds(pos.x, pos.y, size * 1.5f, size);
    hov = bounds.contains(mPos);
    return hov;
  }

  void draw(RenderWindow &win) {
    Color col = hov ? Color::Yellow : Color::White;

    ConvexShape b(6);
    b.setPoint(0, {0, size * 0.3f});
    b.setPoint(1, {size * 0.3f, size * 0.3f});
    b.setPoint(2, {size * 0.7f, 0});
    b.setPoint(3, {size * 0.7f, size});
    b.setPoint(4, {size * 0.3f, size * 0.7f});
    b.setPoint(5, {0, size * 0.7f});
    b.setPosition(pos);
    b.setFillColor(col);
    win.draw(b);

    auto aR = [&](float w, float h, float ox, float oy, float rx, float ry,
                  float rot, Color c) {
      RectangleShape r({w, h});
      r.setOrigin(ox, oy);
      r.setPosition(pos.x + rx, pos.y + ry);
      r.setRotation(rot);
      r.setFillColor(c);
      win.draw(r);
    };
    if (!isMuted) {
      aR(4, size * 0.4f, 2, size * 0.2f, size * 0.9f, size * 0.5f, 0, col);
      aR(4, size * 0.6f, 2, size * 0.3f, size * 1.15f, size * 0.5f, 0, col);
    } else {
      aR(size * 0.6f, 4, size * 0.3f, 2, size * 1.05f, size * 0.5f, 45,
         Color::Red);
      aR(size * 0.6f, 4, size * 0.3f, 2, size * 1.05f, size * 0.5f, -45,
         Color::Red);
    }
  }
};

// --- Main App ---
class JosephusApp {
  RenderWindow win;
  int n = 10, k = 3, start = 1, curIdx = 0, step = 0, aliveCnt = 0,
      lastElim = -1;
  Music bgM;
  SoundBuffer eliBuf, winBuf, horseBuf;
  Sound eliM, winM, horseM;
  Texture bgT, horseNormalT, horseAggroT, flagT;
  Sprite bgS, horseS;
  vector<Sprite> flags;
  bool hasBg = false, hasHorse = false, hasFlag = false;
  Font font, tFont;
  vector<SmokeParticle> smokes;

  Text lblN, lblK, lblS, title, hud, vicTxt;
  UIElement bNm, bNp, bKm, bKp, bSm, bSp, btnStart, tbN, tbK, tbS;
  SpeakerIcon btnMute;
  bool isMuted = false;

  State state = Menu;
  SimState sState = Ready;
  vector<Person> ppl;
  Vector2f ctr = {950, 600};
  float mRad = 450.f, oRad = 550.f;
  ConvexShape hudBg;
  ArrowShape arrow;
  float curRot = 0, tgtRot = 0, curArrL = 450.f;
  string lastHud;
  Clock t, appClock;
  float animP = 0.f;
  float horseShowTime = 0.f;

  void updArrow(float l) { arrow.upd(l); }

  void setup() {
    ppl.clear();
    aliveCnt = n;
    step = 0;
    lastElim = -1;
    curIdx = (start - 1) % n;
    mRad = max(150.f, n * 16.f);
    oRad = mRad + max(100.f, mRad * 0.2f);

    float reqRad = oRad + 50.f;
    float viewH = max(1200.f, reqRad * 2.f);
    float zoom = viewH / 1200.f;

    float cSize = max(35.f * 1.1f, 14.f * zoom);
    float r = cSize / 1.1f;

    for (int i = 0; i < n; i++) {
      Person p;
      p.id = i + 1;
      p.angle = i * 2 * PI / n - PI / 2;
      p.curR = p.tgtR = mRad;
      p.shape.setRadius(r);
      p.shape.setOrigin(r, r);
      p.shape.setOutlineThickness(max(4.f, 2.f * zoom));
      p.cFill = p.tFill = Color(0, 0, 0, 255);
      p.cOut = p.tOut = (p.id == start) ? Color::Yellow : Color::Green;
      p.cTxt = p.tTxt = Color::White;

      p.idText.setFont(font);
      p.idText.setString(to_string(p.id));
      p.idText.setCharacterSize(cSize);
      p.idText.setOutlineColor(Color(0, 0, 0, 200));
      p.idText.setOutlineThickness(1.5f * zoom);

      FloatRect b = p.idText.getLocalBounds();
      p.idText.setOrigin(b.left + b.width / 2.f, b.top + b.height / 2.f);
      ppl.push_back(p);
    }
    tgtRot = curRot = curIdx * 360.f / n - 90.f;
    arrow.setRotation(curRot);
    updArrow(curArrL = mRad * 0.9f);
    sState = Ready;
  }

public:
  JosephusApp() : win(VideoMode(1900, 1200), "Josephus") {
    win.setFramerateLimit(240);
    font.loadFromFile("C:/Windows/Fonts/arial.ttf");
    tFont.loadFromFile("CinzelDecorative-Bold.otf");
    if (bgT.loadFromFile("bg_war.png")) {
      bgS.setTexture(bgT);
      bgS.setScale(1900.f / bgT.getSize().x, 1200.f / bgT.getSize().y);
      hasBg = true;
    }
    auto key = [](Image &img, Color bg) {
      for (unsigned x = 0; x < img.getSize().x; ++x)
        for (unsigned y = 0; y < img.getSize().y; ++y) {
          Color c = img.getPixel(x, y);
          if (abs((int)c.r - bg.r) + abs((int)c.g - bg.g) +
                  abs((int)c.b - bg.b) <
              60)
            img.setPixel(x, y, Color(0, 0, 0, 0));
        }
    };
    Image hN, hA, fImg;
    if (hN.loadFromFile("horse_normal.png") &&
        hA.loadFromFile("horse_aggro.png")) {
      key(hN, hN.getPixel(0, 0));
      key(hA, hA.getPixel(0, 0));
      horseNormalT.loadFromImage(hN);
      horseAggroT.loadFromImage(hA);
      horseS.setTexture(horseNormalT);
      horseS.setOrigin(horseNormalT.getSize().x, horseNormalT.getSize().y);
      horseS.setPosition(1900, 1200);
      horseS.setScale(0.3f, 0.3f);
      hasHorse = true;
    }
    if (fImg.loadFromFile("flag.png")) {
      key(fImg, fImg.getPixel(0, 0));
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
    if (eliBuf.loadFromFile("eli.wav"))
      eliM.setBuffer(eliBuf);
    if (winBuf.loadFromFile("win.wav"))
      winM.setBuffer(winBuf);
    if (horseBuf.loadFromFile("horse.wav"))
      horseM.setBuffer(horseBuf);

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
    btnMute = SpeakerIcon({20, 1140});

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
    arrow.setFillColor(Color::White);
    arrow.setPosition(ctr);
  }

  void run() {
    while (win.isOpen()) {
      Vector2f mPos = win.mapPixelToCoords(Mouse::getPosition(win));
      Event e;
      while (win.pollEvent(e)) {
        if (e.type == Event::Closed)
          win.close();

        if (e.type == Event::MouseButtonPressed && btnMute.update(mPos)) {
          isMuted = !isMuted;
          btnMute.isMuted = isMuted;
          bgM.setVolume(isMuted ? 0.f : 50.f);
          eliM.setVolume(isMuted ? 0.f : 100.f);
          winM.setVolume(isMuted ? 0.f : 100.f);
          horseM.setVolume(isMuted ? 0.f : 100.f);
        }

        if (state == Menu) {
          tbN.handle(e);
          tbK.handle(e);
          tbS.handle(e);
          if (e.type == Event::MouseButtonPressed) {
            UIElement *tbs[] = {&tbN, &tbK, &tbS};
            for (auto t : tbs)
              t->isSel = t->rect.getGlobalBounds().contains(mPos);
            auto val = [](string s) { return stoi(s.empty() ? "1" : s); };
            n = val(tbN.val);
            k = val(tbK.val);
            start = val(tbS.val);
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
            for (auto t : tbs)
              t->updText();

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
          ppl[curIdx].tFill = Color(80, 0, 0, 255);
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
          animP = min(1.f, animP + 0.008f);
          if (animP < 0.3f) {
            ppl[curIdx].tgtScale = 1.0f + (animP / 0.3f) * 0.8f;
          } else {
            ppl[curIdx].tgtScale = 1.0f + (1.0f - (animP - 0.3f) / 0.7f) * 0.8f;
          }

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
        float tLen = (sState == AnimElim) ? oRad * 0.9f : mRad * 0.9f;
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

          if (abs(p.curScale - p.tgtScale) > 0.001f)
            p.curScale += (p.tgtScale - p.curScale) * 0.05f;
          p.shape.setScale(p.curScale, p.curScale);
          p.idText.setScale(p.curScale, p.curScale);
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
      win.setView(win.getDefaultView());
      win.clear({30, 30, 30});
      if (hasBg)
        win.draw(bgS);
      if (state == Menu) {
        RectangleShape ov({1900, 1200});
        ov.setFillColor({0, 0, 0, 180});
        win.draw(ov);
        UIElement *uis[] = {&bNm, &bNp, &tbN, &bKm, &bKp,
                            &tbK, &bSm, &bSp, &tbS, &btnStart};
        for (auto u : uis) {
          u->update(mPos);
          u->draw(win);
        }
        win.draw(title);
        win.draw(lblN);
        win.draw(lblK);
        win.draw(lblS);
      } else {
        sf::View defaultView = win.getDefaultView();
        float reqRad = oRad + 50.f;
        float viewH = reqRad * 2.f;
        if (viewH < 1200.f)
          viewH = 1200.f;
        float viewW = viewH * (1900.f / 1200.f);
        sf::View simView(ctr, {viewW, viewH});
        win.setView(simView);

        float mScale = mRad / 450.f;
        CircleShape cp(mRad + 15.f * mScale);
        cp.setOrigin(mRad + 15.f * mScale, mRad + 15.f * mScale);
        cp.setPosition(ctr);
        cp.setFillColor({0, 0, 0, 100});
        cp.setOutlineThickness(3.f * mScale);
        win.draw(cp);

        for (int i = 0; i < n; i++) {
          if (ppl[i].alive && i != curIdx) {
            win.draw(ppl[i].shape);
            win.draw(ppl[i].idText);
          }
        }
        for (int i = 0; i < n; i++) {
          if (!ppl[i].alive && i != curIdx) {
            win.draw(ppl[i].shape);
            win.draw(ppl[i].idText);
          }
        }
        if (curIdx >= 0 && curIdx < n) {
          win.draw(ppl[curIdx].shape);
          win.draw(ppl[curIdx].idText);
        }
        win.draw(arrow);
        
        float arrS = curArrL / 450.f;
        float dotRad = 20.f * arrS; 
        CircleShape dot(dotRad);
        dot.setOrigin(dotRad, dotRad);
        dot.setPosition(ctr);
        dot.setFillColor(Color::White);
        win.draw(dot);

        if (hasFlag) {
          float gRot = appClock.getElapsedTime().asSeconds() * 20.f;
          float flagScale = 0.15f * mScale;
          float flagDist = 120.f * mScale;
          for (int i = 0; i < 8; i++) {
            auto &f = flags[i];
            float a = (i * 360.f / 8.f + gRot) * PI / 180.f;
            f.setScale(flagScale, flagScale);
            f.setPosition(ctr.x + flagDist * cos(a), ctr.y + flagDist * sin(a));
            f.setRotation(a * 180.f / PI + 90.f);
            win.draw(f);
          }
        }

        win.setView(defaultView);

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
          vicTxt.setPosition(950.f, 600.f);
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

      btnMute.update(mPos);
      btnMute.draw(win);

      win.display();
    }
  }
};

int main() {
  JosephusApp().run();
  return 0;
}