; RUN: opt -load LLVMGenHPVM.so -S -genhpvm <  %s | FileCheck %s
; ModuleID = 'ThreeLevel.allocation.c'
source_filename = "ThreeLevel.allocation.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Root = type { i32*, i64, i32*, i64 }
; CHECK-LABEL: %struct.out.Allocation =

; CHECK-LABEL: void @Launch(
; CHECK: call i8* @llvm.hpvm.launch(i8*
; CHECK-NEXT: call void @llvm.hpvm.wait(i8*

; CHECK-LABEL: i32 @main(
; CHECK: call void @llvm.hpvm.init()
; CHECK-NEXT: tail call void @Launch(
; CHECK-NEXT: call void @llvm.hpvm.cleanup()

; CHECK-LABEL: @Allocation_cloned(
; CHECK: call i8* @llvm.hpvm.malloc(i64

; CHECK-LABEL: @Func1_cloned(

; CHECK-LABEL: @Func3_cloned(
; CHECK: %Func1_cloned.node = call i8* @llvm.hpvm.createNode2D(i8*
; CHECK: %Allocation_cloned.node = call i8* @llvm.hpvm.createNode(i8*
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Allocation_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func1_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func1_cloned.node
; CHECK-NEXT: call i8* @llvm.hpvm.createEdge(i8* %Allocation_cloned.node
; CHECK-NEXTL call i8* @llvm.hpvm.createEdge(i8* %Allocation_cloned.node

; CHECK-LABEL: @Func2_cloned(
; CHECK: %Func3_cloned.node = call i8* @llvm.hpvm.createNode2D(
; CHECK-NEXT:  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node
; CHECK-NEXT:  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node
; CHECK-NEXT:  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node
; CHECK-NEXT:  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node

; CHECK-LABEL: @PipeRoot_cloned(
; CHECK: %Func2_cloned.node = call i8* @llvm.hpvm.createNode(i8*
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node

; Function Attrs: nounwind uwtable
define dso_local void @Func1(i32* %In, i64 %Insize, i32* %Out, i64 %Outsize) #0 {
entry:
  tail call void @__hpvm__hint(i32 2) #5
  tail call void (i32, ...) @__hpvm__attributes(i32 2, i32* %In, i32* %Out, i32 1, i32* %Out) #5
  ret void
}

declare dso_local void @__hpvm__hint(i32) local_unnamed_addr #1

declare dso_local void @__hpvm__attributes(i32, ...) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define dso_local void @Allocation(i64 %block) #0 {
entry:
  %call = tail call i8* @__hpvm__malloc(i64 %block) #5
  tail call void (i32, ...) @__hpvm__return(i32 2, i8* %call, i64 %block) #5
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

declare dso_local i8* @__hpvm__malloc(i64) local_unnamed_addr #1

declare dso_local void @__hpvm__return(i32, ...) local_unnamed_addr #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: nounwind uwtable
define dso_local void @Func3(i32* %In, i64 %Insize, i32* %Out, i64 %Outsize) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #5
  tail call void (i32, ...) @__hpvm__attributes(i32 2, i32* %In, i32* %Out, i32 1, i32* %Out) #5
  %call = tail call i8* (i32, ...) @__hpvm__createNodeND(i32 2, void (i32*, i64, i32*, i64)* nonnull @Func1, i64 3, i64 5) #5
  %call1 = tail call i8* (i32, ...) @__hpvm__createNodeND(i32 0, void (i64)* nonnull @Allocation) #5
  tail call void @__hpvm__bindIn(i8* %call1, i32 1, i32 0, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 2, i32 2, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 3, i32 3, i32 0) #5
  %call2 = tail call i8* @__hpvm__edge(i8* %call1, i8* %call, i32 1, i32 0, i32 0, i32 0) #5
  %call3 = tail call i8* @__hpvm__edge(i8* %call1, i8* %call, i32 1, i32 1, i32 1, i32 0) #5
  ret void
}

declare dso_local i8* @__hpvm__createNodeND(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__bindIn(i8*, i32, i32, i32) local_unnamed_addr #1

declare dso_local i8* @__hpvm__edge(i8*, i8*, i32, i32, i32, i32) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define dso_local void @Func2(i32* %In, i64 %Insize, i32* %Out, i64 %Outsize) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #5
  tail call void (i32, ...) @__hpvm__attributes(i32 2, i32* %In, i32* %Out, i32 1, i32* %Out) #5
  %call = tail call i8* (i32, ...) @__hpvm__createNodeND(i32 2, void (i32*, i64, i32*, i64)* nonnull @Func3, i64 3, i64 5) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 0, i32 0, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 1, i32 1, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 2, i32 2, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 3, i32 3, i32 0) #5
  ret void
}

; Function Attrs: nounwind uwtable
define dso_local void @PipeRoot(i32* %In, i64 %Insize, i32* %Out, i64 %Outsize) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #5
  tail call void (i32, ...) @__hpvm__attributes(i32 2, i32* %In, i32* %Out, i32 1, i32* %Out) #5
  %call = tail call i8* (i32, ...) @__hpvm__createNodeND(i32 0, void (i32*, i64, i32*, i64)* nonnull @Func2) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 0, i32 0, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 1, i32 1, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 2, i32 2, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 3, i32 3, i32 0) #5
  ret void
}

; Function Attrs: noinline nounwind uwtable
define dso_local void @Launch() local_unnamed_addr #3 {
entry:
  %RootArgs = alloca %struct.Root, align 8
  %0 = bitcast %struct.Root* %RootArgs to i8*
  call void @llvm.lifetime.start.p0i8(i64 32, i8* nonnull %0) #5
  %call = tail call noalias i8* @malloc(i64 1024) #5
  %1 = bitcast %struct.Root* %RootArgs to i8**
  store i8* %call, i8** %1, align 8, !tbaa !2
  %Insize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 1
  store i64 1024, i64* %Insize, align 8, !tbaa !8
  %output = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 2
  %call1 = tail call noalias i8* @malloc(i64 1024) #5
  %2 = bitcast i32** %output to i8**
  store i8* %call1, i8** %2, align 8, !tbaa !9
  %Outsize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 3
  store i64 1024, i64* %Outsize, align 8, !tbaa !10
  %call2 = call i8* (i32, ...) @__hpvm__launch(i32 0, void (i32*, i64, i32*, i64)* nonnull @PipeRoot, %struct.Root* nonnull %RootArgs) #5
  call void @__hpvm__wait(i8* %call2) #5
  call void @llvm.lifetime.end.p0i8(i64 32, i8* nonnull %0) #5
  ret void
}

; Function Attrs: nofree nounwind
declare dso_local noalias i8* @malloc(i64) local_unnamed_addr #4

declare dso_local i8* @__hpvm__launch(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__wait(i8*) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define dso_local i32 @main() local_unnamed_addr #0 {
entry:
  tail call void (...) @__hpvm__init() #5
  tail call void @Launch()
  tail call void (...) @__hpvm__cleanup() #5
  ret i32 0
}

declare dso_local void @__hpvm__init(...) local_unnamed_addr #1

declare dso_local void @__hpvm__cleanup(...) local_unnamed_addr #1

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { nofree nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git 3551132592a00cab6c966df508ab511598269f78)"}
!2 = !{!3, !4, i64 0}
!3 = !{!"Root", !4, i64 0, !7, i64 8, !4, i64 16, !7, i64 24}
!4 = !{!"any pointer", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C/C++ TBAA"}
!7 = !{!"long", !5, i64 0}
!8 = !{!3, !7, i64 8}
!9 = !{!3, !4, i64 16}
!10 = !{!3, !7, i64 24}
