package com.moonlab.mooncar;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Shader;
import android.view.MotionEvent;
import android.view.View;

final class SteeringWheel extends View {
    interface Listener {
        void onSteer(int degrees);
    }

    private static final int LIMIT = 80;
    private static final int ACCENT = 0xffdafa53;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path spoke = new Path();
    private final Listener listener;
    private int pointer = -1;
    private float degrees;
    private float previousAngle;

    SteeringWheel(Context context, Listener listener) {
        super(context);
        this.listener = listener;
        setContentDescription("조향 핸들");
    }

    void center() {
        pointer = -1;
        degrees = 0;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float scale = Math.min(getWidth(), getHeight()) / 240f;
        canvas.save();
        canvas.translate(getWidth() / 2f, getHeight() / 2f);
        canvas.scale(scale, scale);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(1);
        paint.setColor(0xff373e4c);
        canvas.drawCircle(0, 0, 116, paint);
        for (int tick = -8; tick <= 8; tick++) {
            canvas.save();
            canvas.rotate(tick * 10);
            paint.setColor(tick == 0 ? ACCENT : 0xff565d69);
            canvas.drawLine(0, -115, 0, tick % 4 == 0 ? -108 : -112, paint);
            canvas.restore();
        }

        canvas.rotate(degrees);
        paint.setStyle(Paint.Style.FILL);
        paint.setShader(new LinearGradient(0, -100, 0, 100, 0xff39404c,
                0xff242a34, Shader.TileMode.CLAMP));
        spoke.reset();
        spoke.moveTo(-92, -36);
        spoke.lineTo(-34, -28);
        spoke.lineTo(34, -28);
        spoke.lineTo(92, -36);
        spoke.lineTo(86, 4);
        spoke.lineTo(34, 10);
        spoke.lineTo(15, 35);
        spoke.lineTo(11, 92);
        spoke.lineTo(-11, 92);
        spoke.lineTo(-15, 35);
        spoke.lineTo(-34, 10);
        spoke.lineTo(-86, 4);
        spoke.close();
        canvas.drawPath(spoke, paint);
        paint.setShader(null);

        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(18);
        paint.setColor(0xff10131a);
        canvas.drawCircle(0, 0, 90, paint);
        paint.setStrokeWidth(12);
        paint.setColor(0xff3d4553);
        canvas.drawCircle(0, 0, 93, paint);
        paint.setStrokeWidth(1.4f);
        paint.setColor(0xff656e7e);
        canvas.drawCircle(0, 0, 98, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(isEnabled() ? ACCENT : 0xff8b956c);
        canvas.drawRoundRect(-4, -99, 4, -86, 2, 2, paint);
        paint.setColor(0xff0f131a);
        canvas.drawCircle(0, 0, 34, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setColor(0xff5b6573);
        canvas.drawCircle(0, 0, 31, paint);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(isEnabled() ? ACCENT : 0xffa0a798);
        paint.setTextSize(13);
        paint.setTextAlign(Paint.Align.CENTER);
        paint.setFakeBoldText(true);
        canvas.drawText("MC", 0, 4.5f, paint);
        paint.setFakeBoldText(false);
        canvas.restore();
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!isEnabled()) return false;

        int index = event.getActionIndex();
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                pointer = event.getPointerId(index);
                previousAngle = touchAngle(event.getX(index), event.getY(index));
                return true;
            case MotionEvent.ACTION_MOVE:
                index = event.findPointerIndex(pointer);
                if (index < 0) return true;
                float x = event.getX(index);
                float y = event.getY(index);
                float angle = touchAngle(x, y);
                if (Math.hypot(x - getWidth() / 2f, y - getHeight() / 2f)
                        > Math.min(getWidth(), getHeight()) * 0.12f) {
                    float delta = (angle - previousAngle + 540f) % 360f - 180f;
                    degrees = Math.max(-LIMIT, Math.min(LIMIT, degrees + delta));
                    listener.onSteer(Math.round(degrees));
                    invalidate();
                }
                previousAngle = angle;
                return true;
            case MotionEvent.ACTION_POINTER_UP:
                if (event.getPointerId(index) != pointer) return true;
                release();
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                release();
                return true;
            default:
                return true;
        }
    }

    private void release() {
        center();
        listener.onSteer(0);
    }

    private float touchAngle(float x, float y) {
        return (float) Math.toDegrees(Math.atan2(x - getWidth() / 2f, getHeight() / 2f - y));
    }
}
