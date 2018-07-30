; ModuleID = 'test1/test1.key.ll'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

declare i16* @acquire(i32)

declare void @llvm.X.i16(i16) nounwind

declare void @llvm.CNOT.i16.i16(i16, i16) nounwind

declare void @release(i16*, i32, i16*, i32)

define i32 @main() nounwind uwtable {
entry:
  %top = alloca [5 x i16], align 2
  %arraydecay = getelementptr inbounds [5 x i16]* %top, i64 0, i64 0
  call void @level1_IP5_IPx_IPx_IPx_DPx_DPx_DPx_DPx(i16* %arraydecay, i32 undef)
  ret i32 0
}

define internal void @level1_IP5_IPx_IPx_IPx_DPx_DPx_DPx_DPx(i16* %in, i32 %n) {
entry.:
  %call. = call i16* @acquire(i32 5) nounwind
  %0 = load i16* %in, align 2
  call void @llvm.X.i16(i16 %0)
  %1 = load i16* %in, align 2
  %2 = load i16* %call., align 2
  call void @llvm.CNOT.i16.i16(i16 %1, i16 %2)
  %arrayidx1..1 = getelementptr inbounds i16* %in, i64 1
  %3 = load i16* %arrayidx1..1, align 2
  %arrayidx3..1 = getelementptr inbounds i16* %call., i64 1
  %4 = load i16* %arrayidx3..1, align 2
  call void @llvm.CNOT.i16.i16(i16 %3, i16 %4)
  %arrayidx1..2 = getelementptr inbounds i16* %in, i64 2
  %5 = load i16* %arrayidx1..2, align 2
  %arrayidx3..2 = getelementptr inbounds i16* %call., i64 2
  %6 = load i16* %arrayidx3..2, align 2
  call void @llvm.CNOT.i16.i16(i16 %5, i16 %6)
  %arrayidx1..3 = getelementptr inbounds i16* %in, i64 3
  %7 = load i16* %arrayidx1..3, align 2
  %arrayidx3..3 = getelementptr inbounds i16* %call., i64 3
  %8 = load i16* %arrayidx3..3, align 2
  call void @llvm.CNOT.i16.i16(i16 %7, i16 %8)
  %arrayidx1..4 = getelementptr inbounds i16* %in, i64 4
  %9 = load i16* %arrayidx1..4, align 2
  %arrayidx3..4 = getelementptr inbounds i16* %call., i64 4
  %10 = load i16* %arrayidx3..4, align 2
  call void @llvm.CNOT.i16.i16(i16 %9, i16 %10)
  %arrayidx4. = getelementptr inbounds i16* %in, i64 4
  %11 = load i16* %arrayidx4., align 2
  call void @llvm.X.i16(i16 %11)
  call void @_Release_level1_IP5_IPx_IPx_IPx_DPx_DPx_DPx_DPx(i16* %in, i32 %n, i16* %in, i32 1, i16* %call., i32 5, i16* %call., i32 5)
  ret void
}

define void @_Release_level1_IP5_IPx_IPx_IPx_DPx_DPx_DPx_DPx(i16* %q0, i32 %q1, i16* %q2, i32 %q3, i16* %q4, i32 %q5, i16* %q6, i32 %q7) {
  %q0.addr = alloca i16*, align 8
  store i16* %q0, i16** %q0.addr, align 8
  %1 = load i16** %q0.addr
  %q1.addr = alloca i32, align 4
  store i32 %q1, i32* %q1.addr, align 4
  %2 = load i32* %q1.addr
  %q2.addr = alloca i16*, align 8
  store i16* %q2, i16** %q2.addr, align 8
  %3 = load i16** %q2.addr
  %q3.addr = alloca i32, align 4
  store i32 %q3, i32* %q3.addr, align 4
  %4 = load i32* %q3.addr
  %q4.addr = alloca i16*, align 8
  store i16* %q4, i16** %q4.addr, align 8
  %5 = load i16** %q4.addr
  %q5.addr = alloca i32, align 4
  store i32 %q5, i32* %q5.addr, align 4
  %6 = load i32* %q5.addr
  %q6.addr = alloca i16*, align 8
  store i16* %q6, i16** %q6.addr, align 8
  %7 = load i16** %q6.addr
  %q7.addr = alloca i32, align 4
  store i32 %q7, i32* %q7.addr, align 4
  %8 = load i32* %q7.addr
  %new = alloca [1 x i16], align 8
  %9 = getelementptr inbounds [1 x i16]* %new, i32 0, i32 0
  %10 = load i16* %9
  %arrayIdx0 = getelementptr inbounds i16* %3, i32 0
  %11 = load i16* %arrayIdx0
  tail call void @llvm.CNOT.i16.i16(i16 %11, i16 %10)
  call void @forward_level1_IP5_IPx_IPx_IPx_DPx_DPx_DPx_DPx_Reverse(i16* %q0, i32 %q1, i16* %q2, i32 %q3, i16* %q4, i32 %q5, i16* %q6, i32 %q7)
  ret void
}

define void @forward_level1_IP5_IPx_IPx_IPx_DPx_DPx_DPx_DPx(i16* %q0, i32 %q1, i16* %q2, i32 %q3, i16* %q4, i32 %q5, i16* %q6, i32 %q7) {
entry.:
  %0 = load i16* %q2, align 2
  call void @llvm.X.i16(i16 %0)
  %1 = load i16* %q2, align 2
  %2 = load i16* %q4, align 2
  call void @llvm.CNOT.i16.i16(i16 %1, i16 %2)
  %arrayidx1..1 = getelementptr inbounds i16* %q2, i64 1
  %3 = load i16* %arrayidx1..1, align 2
  %arrayidx3..1 = getelementptr inbounds i16* %q4, i64 1
  %4 = load i16* %arrayidx3..1, align 2
  call void @llvm.CNOT.i16.i16(i16 %3, i16 %4)
  %arrayidx1..2 = getelementptr inbounds i16* %q2, i64 2
  %5 = load i16* %arrayidx1..2, align 2
  %arrayidx3..2 = getelementptr inbounds i16* %q4, i64 2
  %6 = load i16* %arrayidx3..2, align 2
  call void @llvm.CNOT.i16.i16(i16 %5, i16 %6)
  %arrayidx1..3 = getelementptr inbounds i16* %q2, i64 3
  %7 = load i16* %arrayidx1..3, align 2
  %arrayidx3..3 = getelementptr inbounds i16* %q4, i64 3
  %8 = load i16* %arrayidx3..3, align 2
  call void @llvm.CNOT.i16.i16(i16 %7, i16 %8)
  %arrayidx1..4 = getelementptr inbounds i16* %q2, i64 4
  %9 = load i16* %arrayidx1..4, align 2
  %arrayidx3..4 = getelementptr inbounds i16* %q4, i64 4
  %10 = load i16* %arrayidx3..4, align 2
  call void @llvm.CNOT.i16.i16(i16 %9, i16 %10)
  %arrayidx4. = getelementptr inbounds i16* %q2, i64 4
  %11 = load i16* %arrayidx4., align 2
  call void @llvm.X.i16(i16 %11)
  ret void
}

declare void @_reverse_forward_level1_IP5_IPx_IPx_IPx_DPx_DPx_DPx_DPx(i16*, i32, i16*, i32, i16*, i32, i16*, i32)

declare void @llvm.H.i8(i8) nounwind

declare i1 @llvm.MeasX.i8(i8) nounwind

declare i1 @llvm.MeasZ.i8(i8) nounwind

declare void @llvm.PrepX.i8(i8, i32) nounwind

declare void @llvm.PrepZ.i8(i8, i32) nounwind

declare void @llvm.Rx.i8(i8, double) nounwind

declare void @llvm.Ry.i8(i8, double) nounwind

declare void @llvm.Rz.i8(i8, double) nounwind

declare void @llvm.Sdag.i8(i8) nounwind

declare void @llvm.S.i8(i8) nounwind

declare void @llvm.Tdag.i8(i8) nounwind

declare void @llvm.T.i8(i8) nounwind

declare void @llvm.X.i8(i8) nounwind

declare void @llvm.Y.i8(i8) nounwind

declare void @llvm.Z.i8(i8) nounwind

declare void @llvm.H.i16(i16) nounwind

declare i1 @llvm.MeasX.i16(i16) nounwind

declare i1 @llvm.MeasZ.i16(i16) nounwind

declare void @llvm.PrepX.i16(i16, i32) nounwind

declare void @llvm.PrepZ.i16(i16, i32) nounwind

declare void @llvm.Rx.i16(i16, double) nounwind

declare void @llvm.Ry.i16(i16, double) nounwind

declare void @llvm.Rz.i16(i16, double) nounwind

declare void @llvm.Sdag.i16(i16) nounwind

declare void @llvm.S.i16(i16) nounwind

declare void @llvm.Tdag.i16(i16) nounwind

declare void @llvm.T.i16(i16) nounwind

declare void @llvm.Y.i16(i16) nounwind

declare void @llvm.Z.i16(i16) nounwind

declare void @llvm.CNOT.i8.i8(i8, i8) nounwind

declare void @llvm.CNOT.i8.i16(i8, i16) nounwind

declare void @llvm.CNOT.i16.i8(i16, i8) nounwind

declare void @llvm.Toffoli.i16.i16.i16(i16, i16, i16) nounwind

declare void @llvm.Fredkin.i16.i16.i16(i16, i16, i16) nounwind

declare void @llvm.Toffoli.i16.i16.i8(i16, i16, i8) nounwind

declare void @llvm.Fredkin.i16.i16.i8(i16, i16, i8) nounwind

declare void @llvm.Toffoli.i16.i8.i8(i16, i8, i8) nounwind

declare void @llvm.Fredkin.i16.i8.i8(i16, i8, i8) nounwind

declare void @llvm.Toffoli.i16.i8.i16(i16, i8, i16) nounwind

declare void @llvm.Fredkin.i16.i8.i16(i16, i8, i16) nounwind

declare void @llvm.Toffoli.i8.i16.i16(i8, i16, i16) nounwind

declare void @llvm.Fredkin.i8.i16.i16(i8, i16, i16) nounwind

declare void @llvm.Toffoli.i8.i16.i8(i8, i16, i8) nounwind

declare void @llvm.Fredkin.i8.i16.i8(i8, i16, i8) nounwind

declare void @llvm.Toffoli.i8.i8.i8(i8, i8, i8) nounwind

declare void @llvm.Fredkin.i8.i8.i8(i8, i8, i8) nounwind

declare void @llvm.Toffoli.i8.i8.i16(i8, i8, i16) nounwind

declare void @llvm.Fredkin.i8.i8.i16(i8, i8, i16) nounwind

define void @forward_level1_IP5_IPx_IPx_IPx_DPx_DPx_DPx_DPx_Reverse(i16* %q0, i32 %q1, i16* %q2, i32 %q3, i16* %q4, i32 %q5, i16* %q6, i32 %q7) {
entry.:
  %0 = load i16* %q2, align 2
  %1 = load i16* %q2, align 2
  %2 = load i16* %q4, align 2
  %arrayidx1..1 = getelementptr inbounds i16* %q2, i64 1
  %3 = load i16* %arrayidx1..1, align 2
  %arrayidx3..1 = getelementptr inbounds i16* %q4, i64 1
  %4 = load i16* %arrayidx3..1, align 2
  %arrayidx1..2 = getelementptr inbounds i16* %q2, i64 2
  %5 = load i16* %arrayidx1..2, align 2
  %arrayidx3..2 = getelementptr inbounds i16* %q4, i64 2
  %6 = load i16* %arrayidx3..2, align 2
  %arrayidx1..3 = getelementptr inbounds i16* %q2, i64 3
  %7 = load i16* %arrayidx1..3, align 2
  %arrayidx3..3 = getelementptr inbounds i16* %q4, i64 3
  %8 = load i16* %arrayidx3..3, align 2
  %arrayidx1..4 = getelementptr inbounds i16* %q2, i64 4
  %9 = load i16* %arrayidx1..4, align 2
  %arrayidx3..4 = getelementptr inbounds i16* %q4, i64 4
  %10 = load i16* %arrayidx3..4, align 2
  %arrayidx4. = getelementptr inbounds i16* %q2, i64 4
  %11 = load i16* %arrayidx4., align 2
  call void @llvm.X.i16(i16 %11)
  call void @llvm.CNOT.i16.i16(i16 %9, i16 %10)
  call void @llvm.CNOT.i16.i16(i16 %7, i16 %8)
  call void @llvm.CNOT.i16.i16(i16 %5, i16 %6)
  call void @llvm.CNOT.i16.i16(i16 %3, i16 %4)
  call void @llvm.CNOT.i16.i16(i16 %1, i16 %2)
  call void @llvm.X.i16(i16 %0)
  ret void
}
