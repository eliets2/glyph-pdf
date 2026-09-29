PROGRAM-CONSOLIDATION-2026-09-25 §1.4 — an explicit zoom leaves the fit mode

Pin: TestViewParity::zoomInAfterFitLeavesFitMode (was the expected failure
recorded at the redesign base; the QEXPECT_FAIL marker is now REMOVED and the
assertion is the permanent regression lock, per the XPASS contract written
into the suite header).

Files:
- pass-after-TestViewParity.txt   full transcript with the fix, 15P/0F
- pass-after-x3-totals.txt        3 consecutive full-suite runs, 15P/0F each
- nc-reverted-zoomInAfterFitLeavesFitMode.txt  the fix scoped out of
  PdfViewerWidget::zoomIn/zoomOut — with the QEXPECT_FAIL gone the pin now
  FAILS hard ("Compared values are not the same", FitToWidth != Custom),
  which is precisely the XPASS-forces-the-marker-removal behavior the suite
  documented. (Fail-before: the pre-fix state is the reverted state; the
  original failing behavior was recorded at the redesign base as XFAIL.)

The fix: zoomIn()/zoomOut() switch QPdfView to ZoomMode::Custom before
setZoomFactor (the law setZoomLevel/Actual Size already followed, 06 §4.8).
