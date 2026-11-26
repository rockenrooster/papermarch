# A toy model of PAPERMARCH's rules (GAME_DESIGN 3 and 5.1): it brute-forces every order of the hand-made levels and
# asserts the results GAME_DESIGN 2 and 8.1 state. Run:  python -I -B docs/concept/check_levels.py
# The real simulation (TECH_SPEC 3) must agree with it on these levels; P2's rule tests hold the same numbers.
# Covers camps, villages, shrines, posts, forks, growing camps and the Keep. No mire, depot, key, rival or raid.


def number(site, turn):
    kind, value, per_turn, lo, hi = site
    return max(lo, min(hi, value + per_turn * turn))


def moves(level, s, turn, mine, burnt):
    """Every takeable site: (name, new strength)."""
    out = []
    for name, site in level["sites"].items():
        if name in mine or name in burnt:
            continue
        if not any((a in mine and b == name) or (b in mine and a == name) for a, b in level["roads"]):
            continue
        v, kind = number(site, turn), site[0]
        if kind in ("camp", "keep") and s >= v:
            out.append((name, s + v))
        elif kind == "village":
            out.append((name, s + v))
        elif kind == "shrine":
            out.append((name, s * v))
        elif kind == "post" and s > v:
            out.append((name, s - v))
    return out


def take(level, name, mine, burnt):
    group = level.get("forks", {}).get(name)
    lost = {n for n, g in level.get("forks", {}).items() if g == group and n != name and n not in mine} if group else set()
    return mine | {name}, burnt | lost


def search(level, s=None, turn=0, mine=frozenset(["home"]), burnt=frozenset()):
    """All outcomes from a position: (best final host or None, lowest strength a line can strand at or None)."""
    s = level["start"] if s is None else s
    best, strand = None, None
    options = moves(level, s, turn, mine, burnt)
    if not options:
        return None, s
    for name, s2 in options:
        if level["sites"][name][0] == "keep":
            best = max(best or 0, s2)
            continue
        m2, b2 = take(level, name, mine, burnt)
        b, st = search(level, s2, turn + 1, m2, b2)
        if b is not None:
            best = max(best or 0, b)
        if st is not None:
            strand = st if strand is None else min(strand, st)
    return best, strand


def play(level, order):
    s, turn, mine, burnt = level["start"], 0, frozenset(["home"]), frozenset()
    for name in order:
        s = dict(moves(level, s, turn, mine, burnt))[name]  # KeyError: the move is not legal
        mine, burnt = take(level, name, mine, burnt)
        turn += 1
    return s


def site(kind, value, per_turn=0, lo=1, hi=10**9):
    return (kind, value, per_turn, lo, hi)


HOME = site("home", 0)
LEVELS = {
    1: dict(start=10, sites=dict(home=HOME, a=site("camp", 4), b=site("camp", 9), k=site("keep", 20)),
            roads=[("home", "a"), ("a", "b"), ("b", "k")], best=43, strands=False),
    2: dict(start=10, sites=dict(home=HOME, a=site("camp", 6), b=site("camp", 14), k=site("keep", 28)),
            roads=[("home", "a"), ("home", "b"), ("a", "k"), ("b", "k")], best=58, strands=False),
    3: dict(start=5, sites=dict(home=HOME, v=site("village", 8), c=site("camp", 10), k=site("keep", 22)),
            roads=[("home", "v"), ("home", "c"), ("c", "k")], best=45, strands=False),
    4: dict(start=6, sites=dict(home=HOME, c5=site("camp", 5), sh=site("shrine", 2), c9=site("camp", 9), k=site("keep", 40)),
            roads=[("home", "c5"), ("home", "sh"), ("c5", "c9"), ("sh", "c9"), ("c9", "k")], best=80, strands=True),
    5: dict(start=12, sites=dict(home=HOME, c8=site("camp", 8), p=site("post", 6), v=site("village", 10),
                                 c20=site("camp", 20), c3=site("camp", 3), k=site("keep", 40)),
            roads=[("home", "c8"), ("c8", "p"), ("p", "v"), ("v", "c20"), ("c20", "k"), ("home", "c3")], best=87, strands=False),
    6: dict(start=10, sites=dict(home=HOME, c6=site("camp", 6), left=site("camp", 15), right=site("village", 6), k=site("keep", 30)),
            roads=[("home", "c6"), ("c6", "left"), ("c6", "right"), ("left", "k"), ("right", "k")],
            forks=dict(left=1, right=1), best=61, strands=True),
    12: dict(start=10, sites=dict(home=HOME, a=site("camp", 6), b=site("village", 8), c=site("camp", 14), d=site("shrine", 2),
                                  e=site("post", 5), f=site("camp", 30), g=site("camp", 45), i=site("camp", 12, 3, 1, 45),
                                  k=site("keep", 120)),
             roads=[("home", "a"), ("home", "b"), ("a", "c"), ("b", "d"), ("c", "e"), ("d", "e"), ("e", "f"), ("e", "g"),
                    ("f", "i"), ("g", "i"), ("i", "k")], best=396, strands=False),
}

if __name__ == "__main__":
    for n, level in LEVELS.items():
        best, strand = search(level)
        print(f"level {n:2}: best {best}, {'can strand at ' + str(strand) if strand is not None else 'cannot strand'}")
        assert best == level["best"], (n, best)
        assert (strand is not None) == level["strands"], (n, strand)
    assert play(LEVELS[4], ["sh", "c5", "c9"]) == 26                                   # the shrine first strands at 26
    assert play(LEVELS[6], ["c6", "right"]) == 22                                      # the wrong fork strands at 22
    assert play(LEVELS[12], ["a", "b"]) == 24                                          # the concept frame
    assert number(LEVELS[12]["sites"]["i"], 2) == 18                                   # the tag the frame shows
    assert play(LEVELS[12], ["a", "b", "d", "c", "e", "f", "g", "i", "k"]) == 285      # the obvious line: two stars
    assert play(LEVELS[12], ["a", "b", "c", "e", "f", "g", "i", "d", "k"]) == 396      # the best line
    assert 285 * 100 >= 70 * 396 and 285 < 396
    print("ok")
