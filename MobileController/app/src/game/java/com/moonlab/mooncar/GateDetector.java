package com.moonlab.mooncar;

/** Finds the two adjacent color blocks of the next checkpoint card. */
final class GateDetector {
    private static final int COLUMNS = 64;
    private static final int ROWS = 48;
    private static final int PINK = 1, CYAN = 2, YELLOW = 3;
    private static final int[][] PATTERNS = {{PINK, CYAN}, {CYAN, PINK}, {YELLOW, CYAN}, {CYAN, YELLOW}};

    private final int[] colors = new int[COLUMNS];

    static final class Marker {
        final float x, y, width, height;

        Marker(int left, int right, int top, int bottom) {
            x = (left + right) / (2f * COLUMNS);
            y = (top + bottom) / (2f * ROWS);
            width = (right - left) / (float) COLUMNS;
            height = (bottom - top) / (float) ROWS;
        }
    }

    Marker find(int[] pixels, int width, int height, int gate) {
        int leftColor = PATTERNS[gate - 1][0];
        int rightColor = PATTERNS[gate - 1][1];
        int top = 0, left = 0, right = 0, rows = 0, bestArea = 0;
        Marker best = null;
        for (int row = 0; row <= ROWS; row++) {
            int rowLeft = 0, rowRight = 0;
            if (row < ROWS) {
                int offset = (row * height / ROWS) * width;
                for (int col = 0; col < COLUMNS; col++) {
                    colors[col] = colorOf(pixels[offset + col * width / COLUMNS]);
                }
                for (int col = 0; col < COLUMNS;) {
                    if (colors[col] != leftColor) {
                        col++;
                        continue;
                    }
                    int start = col++;
                    while (col < COLUMNS && colors[col] == leftColor) col++;
                    int leftWidth = col - start;
                    int seam = col;
                    while (col < COLUMNS && col - seam < 2 && colors[col] == 0) col++;
                    int rightStart = col;
                    while (col < COLUMNS && colors[col] == rightColor) col++;
                    int rightWidth = col - rightStart;
                    if (Math.min(leftWidth, rightWidth) >= 3 &&
                            Math.max(leftWidth, rightWidth) <= 2 * Math.min(leftWidth, rightWidth) &&
                            col - start > rowRight - rowLeft) {
                        rowLeft = start;
                        rowRight = col;
                    }
                }
            }
            boolean hasPair = rowRight > rowLeft;
            boolean samePair = hasPair && rows > 0 && Math.abs(rowLeft - left) <= 4 &&
                    Math.abs(rowRight - right) <= 4;
            if (rows > 0 && !samePair) {
                int span = right - left;
                int area = span * rows;
                if (rows >= 4 && rows * 4 >= span && rows <= span && area > bestArea) {
                    best = new Marker(left, right, top, row);
                    bestArea = area;
                }
                rows = 0;
            }
            if (hasPair) {
                if (rows == 0) {
                    top = row;
                    left = rowLeft;
                    right = rowRight;
                }
                rows++;
            }
        }
        return best;
    }

    private static int colorOf(int rgb) {
        int r = (rgb >> 16) & 255, g = (rgb >> 8) & 255, b = rgb & 255;
        if (Math.min(r, b) >= 90 && Math.min(r, b) - g >= 38) return PINK;
        if (Math.min(g, b) >= 90 && Math.min(g, b) - r >= 35) return CYAN;
        if (Math.min(r, g) >= 100 && Math.min(r, g) - b >= 42) return YELLOW;
        return 0;
    }
}
