// WWE '12 PC - Android: look of the app (yellow and black), shared by the launcher, MY SAVES, the layout editor
// and the game's loading screen. Everything is drawn in code (no image or font file): panels, buttons,
// switches, drop-downs, the side menu shapes, caution-tape stripes and the animated loading bar.
package io.github.imrburn.wwe12;

import android.animation.ValueAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.LinearInterpolator;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.TextView;

final class Ui {
    private Ui() {}

    // ---- Palette -------------------------------------------------------------------------------
    static final int BG = Color.rgb(10, 10, 11);          // screen
    static final int SIDE = Color.rgb(15, 15, 16);        // side menu / bottom bar
    static final int SURFACE = Color.rgb(22, 22, 24);     // panels
    static final int SURFACE2 = Color.rgb(36, 36, 39);    // buttons, drop-downs
    static final int LINE = Color.rgb(58, 58, 63);        // thin borders
    static final int YELLOW = Color.rgb(240, 191, 61);    // accent (same as the PC launcher)
    static final int YELLOW_DIM = Color.rgb(150, 118, 34);
    static final int TEXT = Color.rgb(243, 241, 236);
    static final int SOFT = Color.rgb(196, 194, 188);
    static final int MUTED = Color.rgb(150, 148, 142);
    static final int RED = Color.rgb(224, 74, 70);
    static final int ORANGE = Color.rgb(255, 176, 72);
    static final int GREEN = Color.rgb(124, 204, 124);

    // Headings: condensed, bold, italic (a wrestling-poster look) from the phone's own fonts.
    static final Typeface HEADING = Typeface.create("sans-serif-condensed", Typeface.BOLD_ITALIC);
    static final Typeface BODY = Typeface.create("sans-serif", Typeface.NORMAL);

    static int dp(Context c, float v) {
        return (int) (v * c.getResources().getDisplayMetrics().density + 0.5f);
    }

    // ---- Text ----------------------------------------------------------------------------------
    // Old callers pass Color.WHITE / LTGRAY / GRAY: they are mapped to the palette.
    static TextView text(Context c, String s, int sp, int color, boolean bold) {
        TextView t = new TextView(c);
        t.setText(s);
        t.setTextSize(sp);
        t.setTextColor(color == Color.WHITE ? TEXT : color == Color.LTGRAY ? SOFT : color == Color.GRAY ? MUTED : color);
        t.setTypeface(bold ? HEADING : BODY);
        if (bold) t.setLetterSpacing(0.02f);
        t.setLineSpacing(0, 1.12f);
        return t;
    }

    // ---- Panels --------------------------------------------------------------------------------
    // A card whose children are spaced by themselves (no margins to set at each addView).
    static class Card extends LinearLayout {
        final int gap;

        Card(Context c) {
            super(c);
            setOrientation(VERTICAL);
            gap = dp(c, 10);
            int pad = dp(c, 14);
            setPadding(pad, pad, pad, pad);
            GradientDrawable bg = new GradientDrawable();
            bg.setColor(SURFACE);
            bg.setCornerRadius(dp(c, 10));
            bg.setStroke(dp(c, 1), LINE);
            setBackground(bg);
        }

        @Override
        public void addView(View child, int index, ViewGroup.LayoutParams params) {
            if (getChildCount() > 0 && params instanceof LinearLayout.LayoutParams) {
                LinearLayout.LayoutParams lp = (LinearLayout.LayoutParams) params;
                if (lp.topMargin == 0) lp.topMargin = gap;
            }
            super.addView(child, index, params);
        }
    }

    // A titled card added to parent (name = null: no title). Returns the card to fill.
    static LinearLayout panel(Context c, LinearLayout parent, String name) {
        LinearLayout.LayoutParams cp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        if (name != null) {
            LinearLayout head = new LinearLayout(c);
            head.setOrientation(LinearLayout.HORIZONTAL);
            head.setGravity(Gravity.CENTER_VERTICAL);
            View mark = new View(c);
            mark.setBackground(new Slant(YELLOW, YELLOW, dp(c, 4)));
            head.addView(mark, new LinearLayout.LayoutParams(dp(c, 8), dp(c, 16)));
            TextView t = text(c, name, 15, YELLOW, true);
            t.setLetterSpacing(0.06f);
            t.setPadding(dp(c, 8), 0, 0, 0);
            head.addView(t);
            LinearLayout.LayoutParams hp = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
            hp.topMargin = dp(c, 18);
            hp.bottomMargin = dp(c, 8);
            parent.addView(head, hp);
        } else {
            cp.topMargin = dp(c, 18);
        }
        Card card = new Card(c);
        parent.addView(card, cp);
        return card;
    }

    // ---- Buttons -------------------------------------------------------------------------------
    static Button button(Context c, String label, final boolean primary) {
        Button b = new Button(c) {
            @Override
            public void setEnabled(boolean enabled) {
                super.setEnabled(enabled);
                setAlpha(enabled ? 1f : 0.45f);
            }
        };
        b.setText(label);
        b.setAllCaps(false);
        b.setTypeface(HEADING);
        b.setTextSize(primary ? 24 : 15);
        b.setLetterSpacing(0.04f);
        b.setTextColor(primary ? Color.BLACK : TEXT);
        b.setMinHeight(dp(c, primary ? 60 : 48));
        b.setMinimumHeight(dp(c, primary ? 60 : 48));
        b.setPadding(dp(c, 14), 0, dp(c, 14), 0);
        GradientDrawable content = new GradientDrawable();
        content.setCornerRadius(dp(c, 9));
        if (primary) {
            content.setColor(YELLOW);
        } else {
            content.setColor(SURFACE2);
            content.setStroke(dp(c, 1), LINE);
        }
        GradientDrawable mask = new GradientDrawable();
        mask.setColor(Color.WHITE);
        mask.setCornerRadius(dp(c, 9));
        b.setBackground(new RippleDrawable(
                ColorStateList.valueOf(primary ? Color.argb(80, 0, 0, 0) : Color.argb(100, 240, 191, 61)), content, mask));
        b.setStateListAnimator(null);  // no raised shadow
        return b;
    }

    // ---- Switch row (label on the left, switch on the right; the whole row is tappable) --------
    static View switchRow(Context c, String label, boolean checked, CompoundButton.OnCheckedChangeListener l) {
        LinearLayout row = new LinearLayout(c);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setMinimumHeight(dp(c, 44));
        row.addView(text(c, label, 14, TEXT, false), new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));
        final Switch sw = new Switch(c);
        int[][] states = {{android.R.attr.state_checked}, {}};
        sw.setThumbTintList(new ColorStateList(states, new int[] {YELLOW, Color.rgb(190, 190, 190)}));
        sw.setTrackTintList(new ColorStateList(states, new int[] {Color.argb(130, 240, 191, 61), Color.rgb(72, 72, 77)}));
        sw.setChecked(checked);
        sw.setOnCheckedChangeListener(l);
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        sp.leftMargin = dp(c, 10);
        row.addView(sw, sp);
        row.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) { sw.toggle(); }
        });
        return row;
    }

    // ---- Drop-down -----------------------------------------------------------------------------
    static Spinner spinner(final Context c, String[] names) {
        Spinner s = new Spinner(c);
        s.setBackground(box(c, SURFACE2));
        s.setPopupBackgroundDrawable(box(c, SURFACE2));
        s.setPadding(dp(c, 12), 0, dp(c, 10), 0);
        ArrayAdapter<String> a = new ArrayAdapter<String>(c, android.R.layout.simple_spinner_item, names) {
            @Override
            public View getView(int pos, View convert, ViewGroup parent) {
                TextView t = (TextView) super.getView(pos, convert, parent);
                t.setTextColor(TEXT);
                t.setTextSize(15);
                t.setTypeface(BODY);
                t.setPadding(0, dp(c, 11), 0, dp(c, 11));
                t.setCompoundDrawablesWithIntrinsicBounds(null, null, new Arrow(dp(c, 11)), null);
                return t;
            }

            @Override
            public View getDropDownView(int pos, View convert, ViewGroup parent) {
                TextView t = (TextView) super.getDropDownView(pos, convert, parent);
                t.setTextColor(TEXT);
                t.setTextSize(15);
                t.setTypeface(BODY);
                t.setBackgroundColor(SURFACE2);
                t.setPadding(dp(c, 14), dp(c, 12), dp(c, 14), dp(c, 12));
                return t;
            }
        };
        a.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        s.setAdapter(a);
        return s;
    }

    static GradientDrawable box(Context c, int fill) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        g.setCornerRadius(dp(c, 9));
        g.setStroke(dp(c, 1), LINE);
        return g;
    }

    // Panel with a coloured border (the phone warning).
    static GradientDrawable alert(Context c, int stroke) {
        GradientDrawable g = new GradientDrawable();
        g.setColor(SURFACE);
        g.setCornerRadius(dp(c, 10));
        g.setStroke(dp(c, 2), stroke);
        return g;
    }

    // ---- Shapes --------------------------------------------------------------------------------
    // A bar whose right edge is cut at an angle (the side menu items and the title marks).
    static class Slant extends Drawable {
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Path path = new Path();
        final float skew;
        int color, pressedColor;
        boolean pressed;

        Slant(int color, int pressedColor, float skew) {
            this.color = color;
            this.pressedColor = pressedColor;
            this.skew = skew;
        }

        void setColor(int color, int pressedColor) {
            this.color = color;
            this.pressedColor = pressedColor;
            invalidateSelf();
        }

        @Override
        public void draw(Canvas cv) {
            Rect b = getBounds();
            path.reset();
            path.moveTo(b.left, b.top);
            path.lineTo(b.right - skew, b.top);
            path.lineTo(b.right, b.bottom);
            path.lineTo(b.left, b.bottom);
            path.close();
            p.setColor(pressed ? pressedColor : color);
            cv.drawPath(path, p);
        }

        @Override
        public boolean isStateful() { return true; }

        @Override
        protected boolean onStateChange(int[] state) {
            boolean now = false;
            for (int s : state) if (s == android.R.attr.state_pressed) now = true;
            if (now == pressed) return false;
            pressed = now;
            invalidateSelf();
            return true;
        }

        @Override public void setAlpha(int alpha) {}
        @Override public void setColorFilter(ColorFilter cf) {}
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    // Small down-pointing triangle for the drop-downs.
    static class Arrow extends Drawable {
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Path path = new Path();
        final int size;

        Arrow(int size) {
            this.size = size;
            p.setColor(YELLOW);
        }

        @Override
        public void draw(Canvas cv) {
            Rect b = getBounds();
            path.reset();
            path.moveTo(b.left, b.top + b.height() * 0.25f);
            path.lineTo(b.right, b.top + b.height() * 0.25f);
            path.lineTo(b.centerX(), b.top + b.height() * 0.8f);
            path.close();
            cv.drawPath(path, p);
        }

        @Override public int getIntrinsicWidth() { return size; }
        @Override public int getIntrinsicHeight() { return size; }
        @Override public void setAlpha(int alpha) {}
        @Override public void setColorFilter(ColorFilter cf) {}
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }

    // Caution tape: diagonal yellow stripes on black. phase moves them (the loading bar).
    static class Stripes extends Drawable {
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        final Path path = new Path();
        final float width;
        float phase;

        Stripes(float width) { this.width = width; }

        void setPhase(float phase) { this.phase = phase; }

        @Override
        public void draw(Canvas cv) {
            Rect b = getBounds();
            cv.save();
            cv.clipRect(b);
            p.setColor(Color.rgb(16, 16, 17));
            cv.drawRect(b, p);
            p.setColor(YELLOW);
            float h = b.height(), step = width * 2;
            for (float x = b.left - h - step + (phase % step); x < b.right + step; x += step) {
                path.reset();
                path.moveTo(x, b.bottom);
                path.lineTo(x + width, b.bottom);
                path.lineTo(x + width + h, b.top);
                path.lineTo(x + h, b.top);
                path.close();
                cv.drawPath(path, p);
            }
            cv.restore();
        }

        @Override public void setAlpha(int alpha) {}
        @Override public void setColorFilter(ColorFilter cf) {}
        @Override public int getOpacity() { return PixelFormat.OPAQUE; }
    }

    // A thin strip of caution tape, as a divider under titles.
    static View stripe(Context c, int heightDp) {
        View v = new View(c);
        v.setBackground(new Stripes(dp(c, heightDp * 1.4f)));
        return v;
    }

    // ---- Loading -------------------------------------------------------------------------------
    // The moving caution-tape bar: indeterminate, runs while it is on screen.
    static class LoadingBar extends View {
        final Stripes stripes;
        final Path clip = new Path();
        final RectF rect = new RectF();
        final Paint border = new Paint(Paint.ANTI_ALIAS_FLAG);
        final float step;
        ValueAnimator anim;

        LoadingBar(Context c) {
            super(c);
            stripes = new Stripes(dp(c, 11));
            step = dp(c, 22);
            border.setStyle(Paint.Style.STROKE);
            border.setStrokeWidth(dp(c, 1.5f));
            border.setColor(YELLOW_DIM);
        }

        @Override
        protected void onDraw(Canvas cv) {
            float r = getHeight() / 2f;
            rect.set(0, 0, getWidth(), getHeight());
            clip.reset();
            clip.addRoundRect(rect, r, r, Path.Direction.CW);
            cv.save();
            cv.clipPath(clip);
            stripes.setBounds(0, 0, getWidth(), getHeight());
            stripes.draw(cv);
            cv.restore();
            rect.inset(border.getStrokeWidth() / 2, border.getStrokeWidth() / 2);
            cv.drawRoundRect(rect, r, r, border);
        }

        @Override
        protected void onAttachedToWindow() {
            super.onAttachedToWindow();
            anim = ValueAnimator.ofFloat(0f, 1f);
            anim.setDuration(900);
            anim.setRepeatCount(ValueAnimator.INFINITE);
            anim.setInterpolator(new LinearInterpolator());
            anim.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
                public void onAnimationUpdate(ValueAnimator a) {
                    stripes.setPhase(((Float) a.getAnimatedValue()) * step);
                    invalidate();
                }
            });
            anim.start();
        }

        @Override
        protected void onDetachedFromWindow() {
            if (anim != null) anim.cancel();
            super.onDetachedFromWindow();
        }
    }

    // Full-screen loading overlay: title, moving bar, a tip that changes every few seconds.
    // It swallows touches. dismiss() fades it out and removes it.
    static class Loading extends FrameLayout {
        TextView tip;
        String[] tips;
        final Handler handler = new Handler(Looper.getMainLooper());
        int index;
        boolean dismissing;
        final Runnable rotate = new Runnable() {
            public void run() {
                if (dismissing || !isAttachedToWindow() || tips.length < 2) return;
                index = (index + 1) % tips.length;
                tip.animate().alpha(0f).setDuration(200).withEndAction(new Runnable() {
                    public void run() {
                        tip.setText(tips[index]);
                        tip.animate().alpha(1f).setDuration(200).start();
                    }
                }).start();
                handler.postDelayed(this, 4500);
            }
        };

        Loading(Context c, String title, String[] tips) {
            super(c);
            this.tips = tips;
            setBackgroundColor(Color.argb(250, 10, 10, 11));
            setClickable(true);
            LinearLayout col = new LinearLayout(c);
            col.setOrientation(LinearLayout.VERTICAL);
            col.setGravity(Gravity.CENTER_HORIZONTAL);
            int pad = dp(c, 28);
            col.setPadding(pad, pad, pad, pad);
            TextView brand = text(c, "WWE '12 PC", 15, MUTED, true);
            brand.setLetterSpacing(0.2f);
            col.addView(brand);
            TextView t = text(c, title, 36, YELLOW, true);
            t.setGravity(Gravity.CENTER);
            LinearLayout.LayoutParams tp = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
            tp.topMargin = dp(c, 6);
            col.addView(t, tp);
            LinearLayout.LayoutParams bp = new LinearLayout.LayoutParams(dp(c, 280), dp(c, 14));
            bp.topMargin = dp(c, 20);
            col.addView(new LoadingBar(c), bp);
            tip = text(c, tips.length > 0 ? tips[0] : "", 13, SOFT, false);
            tip.setGravity(Gravity.CENTER);
            tip.setMaxWidth(dp(c, 340));
            LinearLayout.LayoutParams tipp = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
            tipp.topMargin = dp(c, 24);
            col.addView(tip, tipp);
            addView(col, new FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.CENTER));
        }

        void startTips() {
            handler.removeCallbacks(rotate);
            handler.postDelayed(rotate, 4500);
        }

        void dismiss() {
            if (dismissing) return;
            dismissing = true;
            handler.removeCallbacks(rotate);
            animate().alpha(0f).setDuration(300).withEndAction(new Runnable() {
                public void run() {
                    ViewGroup p = (ViewGroup) getParent();
                    if (p != null) p.removeView(Loading.this);
                }
            }).start();
        }
    }
}
