; RUN: opt -load LLVMGenHPVM.so -S -genhpvm <  %s | FileCheck %s
; ModuleID = 'TwoLaunch.c'
source_filename = "TwoLaunch.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define dso_local void @PipeRoot1(i32* %In, i32* %Out) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #4
  tail call void (i32, ...) @__hpvm__attributes(i32 1, i32* %In, i32 1, i32* %Out) #4
  tail call void (i32, ...) @__hpvm__return(i32 1, i32* %Out) #4
  ret void
}

declare dso_local void @__hpvm__hint(i32) local_unnamed_addr #1

declare dso_local void @__hpvm__attributes(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__return(i32, ...) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define dso_local void @PipeRoot2(i32* %In, i32* %Out) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #4
  tail call void (i32, ...) @__hpvm__attributes(i32 1, i32* %In, i32 1, i32* %Out) #4
  tail call void (i32, ...) @__hpvm__return(i32 1, i32* %Out) #4
  ret void
}

; Function Attrs: nounwind uwtable
define dso_local i32 @main() local_unnamed_addr #0 {
; CHECK-LABEL: i32 @main(
; CHECK: call void @llvm.hpvm.init()
; CHECK: call i8* @llvm.hpvm.launch(i8* bitcast (%struct.out.PipeRoot1 (i32*, i32*)* @PipeRoot1_cloned to i8*), i8* %call, i1 false)
; CHECK-NEXT: call i8* @llvm.hpvm.launch(i8* bitcast (%struct.out.PipeRoot2 (i32*, i32*)* @PipeRoot2_cloned to i8*), i8* %call, i1 false) 
; CHECK-NEXT: call void @llvm.hpvm.wait(i8*
; CHECK-NEXT: call void @llvm.hpvm.wait(i8*
entry:
  %In = alloca i32, align 4
  %Out = alloca i32, align 4
  %0 = bitcast i32* %In to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #4
  %1 = bitcast i32* %Out to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #4
  %call = tail call noalias i8* @malloc(i64 16) #4
  %input = bitcast i8* %call to i32**
  store i32* %In, i32** %input, align 8, !tbaa !2
  %output = getelementptr inbounds i8, i8* %call, i64 8
  %2 = bitcast i8* %output to i32**
  store i32* %Out, i32** %2, align 8, !tbaa !7
  call void (...) @__hpvm__init() #4
  %call1 = call i8* (i32, ...) @__hpvm__launch(i32 0, void (i32*, i32*)* nonnull @PipeRoot1, i8* %call) #4
  %call2 = call i8* (i32, ...) @__hpvm__launch(i32 0, void (i32*, i32*)* nonnull @PipeRoot2, i8* %call) #4
  call void @__hpvm__wait(i8* %call1) #4
  call void @__hpvm__wait(i8* %call2) #4
  call void (...) @__hpvm__cleanup() #4
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #4
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #4
  ret i32 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: nofree nounwind
declare dso_local noalias i8* @malloc(i64) local_unnamed_addr #3

declare dso_local void @__hpvm__init(...) local_unnamed_addr #1

declare dso_local i8* @__hpvm__launch(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__wait(i8*) local_unnamed_addr #1

declare dso_local void @__hpvm__cleanup(...) local_unnamed_addr #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { nofree nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #4 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git 6690f9e7e8b46b96aea222d3e85315cd63545953)"}
!2 = !{!3, !4, i64 0}
!3 = !{!"Root", !4, i64 0, !4, i64 8}
!4 = !{!"any pointer", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C/C++ TBAA"}
!7 = !{!3, !4, i64 8}
