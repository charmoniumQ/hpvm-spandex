; RUN: opt -load LLVMGenHPVM.so -S -genhpvm < %s
; ModuleID = 'ThreeLevel.opt.c'
source_filename = "ThreeLevel.opt.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Root = type { i32*, i64, i32*, i64 }

; Function Attrs: nounwind uwtable
define dso_local void @Func1(i32* %In, i64 %Insize, i32* %Out, i64 %Outsize) #0 {
entry:
  tail call void @__hpvm__hint(i32 2) #5
  tail call void (i32, ...) @__hpvm__attributes(i32 2, i32* %In, i32* %Out, i32 1, i32* %Out) #5
  %call = tail call i8* (...) @__hpvm__getNode() #5
  %call1 = tail call i8* @__hpvm__getParentNode(i8* %call) #5
  %call2 = tail call i64 @__hpvm__getNodeInstanceID_x(i8* %call) #5
  %call3 = tail call i64 @__hpvm__getNodeInstanceID_y(i8* %call) #5
  %call5 = tail call i64 @__hpvm__getNodeInstanceID_x(i8* %call1) #5
  %call7 = tail call i64 @__hpvm__getNodeInstanceID_y(i8* %call1) #5
  %call9 = tail call i64 @__hpvm__getNumNodeInstances_x(i8* %call) #5
  %call11 = tail call i64 @__hpvm__getNumNodeInstances_y(i8* %call) #5
  %mul = mul i64 %call9, %call5
  %add = add i64 %mul, %call2
  %arrayidx = getelementptr inbounds i32, i32* %In, i64 3
  %0 = load i32, i32* %arrayidx, align 4, !tbaa !2
  %sext = shl i64 %add, 32
  %idxprom = ashr exact i64 %sext, 32
  %arrayidx15 = getelementptr inbounds i32, i32* %Out, i64 %idxprom
  %1 = load i32, i32* %arrayidx15, align 4, !tbaa !2
  %add16 = add nsw i32 %1, %0
  store i32 %add16, i32* %arrayidx15, align 4, !tbaa !2
  ret void
}

declare dso_local void @__hpvm__hint(i32) local_unnamed_addr #1

declare dso_local void @__hpvm__attributes(i32, ...) local_unnamed_addr #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

declare dso_local i8* @__hpvm__getNode(...) local_unnamed_addr #1

declare dso_local i8* @__hpvm__getParentNode(i8*) local_unnamed_addr #1

declare dso_local i64 @__hpvm__getNodeInstanceID_x(i8*) local_unnamed_addr #1

declare dso_local i64 @__hpvm__getNodeInstanceID_y(i8*) local_unnamed_addr #1

declare dso_local i64 @__hpvm__getNumNodeInstances_x(i8*) local_unnamed_addr #1

declare dso_local i64 @__hpvm__getNumNodeInstances_y(i8*) local_unnamed_addr #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: nounwind uwtable
define dso_local void @Func3(i32* %In, i64 %Insize, i32* %Out, i64 %Outsize) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #5
  tail call void (i32, ...) @__hpvm__attributes(i32 2, i32* %In, i32* %Out, i32 1, i32* %Out) #5
  %call = tail call i8* (i32, ...) @__hpvm__createNodeND(i32 2, void (i32*, i64, i32*, i64)* nonnull @Func1, i64 3, i64 5) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 0, i32 0, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 1, i32 1, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 2, i32 2, i32 0) #5
  tail call void @__hpvm__bindIn(i8* %call, i32 3, i32 3, i32 0) #5
  ret void
}

declare dso_local i8* @__hpvm__createNodeND(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__bindIn(i8*, i32, i32, i32) local_unnamed_addr #1

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
  store i8* %call, i8** %1, align 8, !tbaa !6
  %Insize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 1
  store i64 1024, i64* %Insize, align 8, !tbaa !10
  %output = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 2
  %call1 = tail call noalias i8* @malloc(i64 1024) #5
  %2 = bitcast i32** %output to i8**
  store i8* %call1, i8** %2, align 8, !tbaa !11
  %Outsize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 3
  store i64 1024, i64* %Outsize, align 8, !tbaa !12
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

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #4 = { nofree nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git 3551132592a00cab6c966df508ab511598269f78)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"int", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!7, !8, i64 0}
!7 = !{!"Root", !8, i64 0, !9, i64 8, !8, i64 16, !9, i64 24}
!8 = !{!"any pointer", !4, i64 0}
!9 = !{!"long", !4, i64 0}
!10 = !{!7, !9, i64 8}
!11 = !{!7, !8, i64 16}
!12 = !{!7, !9, i64 24}
