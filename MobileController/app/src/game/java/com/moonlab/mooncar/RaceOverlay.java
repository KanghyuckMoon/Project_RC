package com.moonlab.mooncar;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.view.View;

import java.util.Locale;

final class RaceOverlay extends View {
    private static final int[] COLORS = {0xff77e5d2, 0xffff835e, 0xffad9aff, 0xfff2cb68};
    private static final String[] RACERS = {"나", "코멧", "벡터", "솔라"};
    private static final int PANEL = 0xe6131c2a;
    private static final int WHITE = 0xfff4f6f8;
    private static final int MUTED = 0xffc0cada;
    private static final float WIDTH = 480;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path body = new Path();
    private final RectF video = new RectF();
    private final Race race;
    private GateDetector.Marker marker;
    private float aspect;
    private long now;

    RaceOverlay(Context context, Race race) {
        super(context);
        this.race = race;
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    void setVideoAspect(float aspect) {
        if (aspect == this.aspect) return;
        this.aspect = aspect;
        invalidate();
    }

    void update(long now, GateDetector.Marker marker) {
        this.now = now;
        this.marker = marker;
        invalidate();
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (aspect <= 0 || getWidth() == 0) return;

        float scale = getWidth() / WIDTH;
        float height = getHeight() / scale;
        float videoWidth = Math.min(WIDTH, height * aspect);
        float videoHeight = videoWidth / aspect;
        video.set((WIDTH - videoWidth) / 2, (height - videoHeight) / 2,
                (WIDTH + videoWidth) / 2, (height + videoHeight) / 2);
        canvas.save();
        canvas.scale(scale, scale);
        Race.State state = race.state();
        if (state == Race.State.COUNTDOWN || state == Race.State.RUNNING || state == Race.State.FINISHED) {
            long elapsed = race.elapsed(now);
            Integer[] order = race.standings(elapsed);
            if (state != Race.State.FINISHED) drawRivals(canvas, elapsed);
            drawStandings(canvas, order);
            rounded(canvas, WIDTH - 118, 8, WIDTH - 8, 51, 8, PANEL);
            text(canvas, "LAP " + race.lap() + " / " + Race.LAPS, WIDTH - 63, 24, 12, MUTED, true);
            text(canvas, clock(elapsed), WIDTH - 63, 43, 17, WHITE, true);
            if (state == Race.State.COUNTDOWN) {
                centerMessage(canvas, height, Integer.toString(race.countdown(now)), "출발 준비");
            } else if (state == Race.State.FINISHED) {
                int position = 0;
                while (order[position] != 0) position++;
                centerMessage(canvas, height, (position + 1) + "위 / " + Race.RACERS, "완주  " + clock(elapsed));
            } else {
                String next = race.expectedGate() == Race.GATES ? "다음 · 결승선 4" : "다음 · 체크포인트 " + race.expectedGate();
                rounded(canvas, 155, height - 32, 325, height - 7, 7, PANEL);
                text(canvas, next, WIDTH / 2, height - 15, 13, marker == null ? WHITE : COLORS[0], true);
            }
        } else {
            String message = state == Race.State.ABORTED ? "레이스 중단 · 출발선에서 다시 시작"
                    : "3랩  ·  라이벌 3대";
            rounded(canvas, 104, height - 34, 376, height - 8, 7, PANEL);
            text(canvas, message, WIDTH / 2, height - 16, 13, WHITE, true);
        }
        canvas.restore();
    }

    private void drawStandings(Canvas canvas, Integer[] order) {
        rounded(canvas, 8, 8, 110, 93, 8, PANEL);
        for (int row = 0; row < order.length; row++) {
            int racer = order[row];
            float y = 27 + 18 * row;
            paint.setColor(COLORS[racer]);
            canvas.drawCircle(23, y - 4, 3, paint);
            text(canvas, (row + 1) + "   " + RACERS[racer], 34, y, 13, COLORS[racer], false);
        }
    }

    private void drawRivals(Canvas canvas, long elapsed) {
        canvas.save();
        canvas.clipRect(video);
        float anchorX = video.centerX();
        float anchorY = video.top + video.height() * .52f;
        if (marker != null) {
            anchorX = video.left + video.width() * marker.x;
            anchorY = video.top + video.height() * Math.min(.72f, marker.y + marker.height * .55f);
        }
        // The visible checkpoint supplies a perspective reference for nearby rivals.
        for (int racer = 1; racer < Race.RACERS; racer++) {
            double position = race.rivalProgress(racer, elapsed);
            double gap = position - race.playerProgress();
            if (position >= Race.FINISH || gap < -.35 || gap > 1.45) continue;
            float depth = (float) Math.max(0, Math.min(1, (gap + .35) / 1.8));
            float y = anchorY + (video.bottom - anchorY) * (.73f - .70f * depth) - 6;
            float lane = (racer - 2) * .24f;
            float x = anchorX + video.width() * lane * (1 - .55f * depth);
            float size = video.height() * (.24f - .15f * depth);
            x = Math.max(video.left + size, Math.min(video.right - size, x));
            float bounce = (float) Math.sin(elapsed / 165.0 + racer) * (1 - depth);
            drawKart(canvas, racer, x, y + bounce, size);
        }
        canvas.restore();
    }

    private void drawKart(Canvas canvas, int racer, float x, float y, float size) {
        canvas.save();
        canvas.translate(x, y);
        canvas.scale(size / 40, size / 40);
        paint.setColor(0x60101620);
        canvas.drawOval(-26, -2, 26, 10, paint);
        rounded(canvas, -23, -13, -14, 6, 3, 0xff151a23);
        rounded(canvas, 14, -13, 23, 6, 3, 0xff151a23);
        body.reset();
        body.moveTo(-20, 0);
        body.lineTo(-16, -12);
        body.lineTo(-9, -17);
        body.lineTo(9, -17);
        body.lineTo(16, -12);
        body.lineTo(20, 0);
        body.close();
        paint.setColor(COLORS[racer]);
        canvas.drawPath(body, paint);
        rounded(canvas, -10, -23, 10, -12, 5, 0xff273243);
        rounded(canvas, -19, -11, 19, -7, 2, COLORS[racer]);
        rounded(canvas, -15, -5, -8, -2, 1, WHITE);
        rounded(canvas, 8, -5, 15, -2, 1, WHITE);
        canvas.restore();
        text(canvas, RACERS[racer], x, y - size * .68f, 11, WHITE, true);
    }

    private void centerMessage(Canvas canvas, float height, String title, String subtitle) {
        float mid = height / 2;
        rounded(canvas, 150, mid - 44, 330, mid + 43, 12, PANEL);
        text(canvas, title, WIDTH / 2, mid + 1, 31, COLORS[0], true);
        text(canvas, subtitle, WIDTH / 2, mid + 25, 13, WHITE, true);
    }

    private void rounded(Canvas canvas, float left, float top, float right, float bottom, float radius, int color) {
        paint.setColor(color);
        canvas.drawRoundRect(left, top, right, bottom, radius, radius, paint);
    }

    private void text(Canvas canvas, String value, float x, float y, float size, int color, boolean centered) {
        paint.setColor(color);
        paint.setTextSize(size);
        paint.setTextAlign(centered ? Paint.Align.CENTER : Paint.Align.LEFT);
        paint.setTypeface(Typeface.DEFAULT_BOLD);
        canvas.drawText(value, x, y, paint);
    }

    private static String clock(long millis) {
        return String.format(Locale.ROOT, "%d:%02d.%d", millis / 60_000, millis / 1000 % 60, millis / 100 % 10);
    }
}
