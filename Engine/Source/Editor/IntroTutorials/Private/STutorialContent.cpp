// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "IntroTutorialsPrivatePCH.h"
#include "STutorialContent.h"
#include "STutorialEditableText.h"
#include "IntroTutorials.h"
#include "TutorialText.h"
#include "EngineAnalytics.h"
#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"

#define LOCTEXT_NAMESPACE "STutorialContent"

namespace TutorialConstants
{
	const float BorderPulseAnimationLength = 1.0f;
	const float BorderIntroAnimationLength = 0.5f;
	const float ContentIntroAnimationLength = 0.5f;
	const float MinBorderOpacity = 0.1f;
	const float ShadowScale = 8.0f;
	const float MaxBorderOffset = 8.0f;
	const FMargin BorderSizeStandalone(24.0f, 24.0f);
	const FMargin BorderSize(24.0f, 24.0f, 24.0f, 42.0f);
}

const float ContentOffset = 10.0f;

void STutorialContent::Construct(const FArguments& InArgs, UEditorTutorial* InTutorial, const FTutorialContent& InContent)
{
	bIsVisible = Anchor.Type == ETutorialAnchorIdentifier::None;

	Tutorial = InTutorial;

	VerticalAlignment = InArgs._VAlign;
	HorizontalAlignment = InArgs._HAlign;
	WidgetOffset = InArgs._Offset;
	bIsStandalone = InArgs._IsStandalone;
	OnClosed = InArgs._OnClosed;
	OnNextClicked = InArgs._OnNextClicked;
	OnHomeClicked = InArgs._OnHomeClicked;
	OnBackClicked = InArgs._OnBackClicked;
	IsBackEnabled = InArgs._IsBackEnabled;
	IsHomeEnabled = InArgs._IsHomeEnabled;
	IsNextEnabled = InArgs._IsNextEnabled;
	Anchor = InArgs._Anchor;
	bAllowNonWidgetContent = InArgs._AllowNonWidgetContent;
	OnWasWidgetDrawn = InArgs._OnWasWidgetDrawn;

	BorderIntroAnimation.AddCurve(0.0f, TutorialConstants::BorderIntroAnimationLength, ECurveEaseFunction::CubicOut);
	BorderPulseAnimation.AddCurve(0.0f, TutorialConstants::BorderPulseAnimationLength, ECurveEaseFunction::Linear);
	BorderIntroAnimation.Play();
	BorderPulseAnimation.Play();

	ContentIntroAnimation.AddCurve(0.0f, TutorialConstants::ContentIntroAnimationLength, ECurveEaseFunction::Linear);
	ContentIntroAnimation.Play();

	if (InContent.Text.IsEmpty() == true)
	{
		ChildSlot
		[
			SAssignNew(ContentWidget, SBorder)
			.Visibility(EVisibility::SelfHitTestInvisible)
		];
		return;
	}

	ChildSlot
	[
		SNew(SFxWidget)
		.Visibility(EVisibility::SelfHitTestInvisible)
		.RenderScale(this, &STutorialContent::GetAnimatedZoom)
		.RenderScaleOrigin(FVector2D(0.5f, 0.5f))
		[
			SNew(SOverlay)
			.Visibility(this, &STutorialContent::GetVisibility)
			+SOverlay::Slot()
			[
				SAssignNew(ContentWidget, SBorder)

				// Add more padding if the content is to be displayed centrally (i.e. not on a widget)
				.Padding(bIsStandalone ? TutorialConstants::BorderSizeStandalone : TutorialConstants::BorderSize)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.BorderImage(FEditorStyle::GetBrush("Tutorials.Border"))
				.BorderBackgroundColor(this, &STutorialContent::GetBackgroundColor)
				.ForegroundColor(FCoreStyle::Get().GetSlateColor("InvertedForeground"))
				[
					SNew(SFxWidget)
					.RenderScale(this, &STutorialContent::GetInverseAnimatedZoom)
					.RenderScaleOrigin(FVector2D(0.5f, 0.5f))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.MaxWidth(600.0f)
							.VAlign(VAlign_Center)
							[
								GenerateContentWidget(InContent, InArgs._WrapTextAt, DocumentationPage)
							]
						]
					]
				]
			]
			+SOverlay::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Right)
			.Padding(16.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(2.0f)
				[
					SNew(SComboButton)
					.ToolTipText(LOCTEXT("MoreOptionsTooltip", "More Options"))
					.Visibility(this, &STutorialContent::GetMenuButtonVisibility)
					.ButtonStyle(&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Tutorials.Content.Button"))
					.ContentPadding(0.0f)
					.OnGetMenuContent(FOnGetContent::CreateSP(this, &STutorialContent::HandleGetMenuContent))
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(0.0f)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("QuitStandaloneTooltip", "Close this Message"))
					.OnClicked(this, &STutorialContent::OnCloseButtonClicked)
					.Visibility(this, &STutorialContent::GetCloseButtonVisibility)
					.ButtonStyle(&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Tutorials.Content.Button"))
					.ContentPadding(0.0f)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("Symbols.X"))
						.ColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
					]
				]
			]
			+SOverlay::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Right)
			.Padding(8.0f)
			[
				SAssignNew(NextButton, SButton)
				.ToolTipText(this, &STutorialContent::GetNextButtonTooltip)
				.OnClicked(this, &STutorialContent::HandleNextClicked)
				.Visibility(this, &STutorialContent::GetMenuButtonVisibility)
				.ButtonStyle(&FEditorStyle::Get().GetWidgetStyle<FButtonStyle>("Tutorials.Navigation.Button"))
				.ContentPadding(0.0f)
				[
					SNew(SBox)
					.Padding(8.0f)
					[
						SNew(SImage)
						.Image(this, &STutorialContent::GetNextButtonBrush)
						.ColorAndOpacity(this, &STutorialContent::GetNextButtonColor)
					]
				]
			]
		]
	];
}

static void GetAnimationValues(bool bIsIntro, float InAnimationProgress, float& OutAlphaFactor, float& OutPulseFactor, FLinearColor& OutShadowTint, FLinearColor& OutBorderTint)
{
	if(bIsIntro)
	{
		OutAlphaFactor = InAnimationProgress;
		OutPulseFactor = (1.0f - OutAlphaFactor) * 50.0f;
		OutShadowTint = FLinearColor(1.0f, 1.0f, 0.0f, OutAlphaFactor);
		OutBorderTint = FLinearColor(1.0f, 1.0f, 0.0f, OutAlphaFactor * OutAlphaFactor);
	}
	else
	{
		OutAlphaFactor = 1.0f - (0.5f + (FMath::Cos(2.0f * PI * InAnimationProgress) * 0.5f));
		OutPulseFactor = 0.5f + (FMath::Cos(2.0f * PI * InAnimationProgress) * 0.5f);
		OutShadowTint = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
		OutBorderTint = FLinearColor(1.0f, 1.0f, 0.0f, TutorialConstants::MinBorderOpacity + ((1.0f - TutorialConstants::MinBorderOpacity) * OutAlphaFactor));
	}
}

int32 STutorialContent::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	CachedContentGeometry = AllottedGeometry;
	CachedContentGeometry.AppendTransform(FSlateLayoutTransform(OutDrawElements.GetWindow()->GetPositionInScreen()));

	if(bIsVisible && Anchor.Type != ETutorialAnchorIdentifier::None && Anchor.bDrawHighlight)
	{
		float AlphaFactor;
		float PulseFactor;
		FLinearColor ShadowTint;
		FLinearColor BorderTint;
		GetAnimationValues(BorderIntroAnimation.IsPlaying(),  BorderIntroAnimation.IsPlaying() ? BorderIntroAnimation.GetLerp() : BorderPulseAnimation.GetLerpLooping(), AlphaFactor, PulseFactor, ShadowTint, BorderTint);

		const FSlateBrush* ShadowBrush = FCoreStyle::Get().GetBrush(TEXT("Tutorials.Shadow"));
		const FSlateBrush* BorderBrush = FCoreStyle::Get().GetBrush(TEXT("Tutorials.Border"));
					
		const FGeometry& WidgetGeometry = CachedGeometry;
		const FVector2D WindowSize = OutDrawElements.GetWindow()->GetSizeInScreen();

		// We should be clipped by the window size, not our containing widget, as we want to draw outside the widget
		FSlateRect WindowClippingRect(0.0f, 0.0f, WindowSize.X, WindowSize.Y);

		FPaintGeometry ShadowGeometry((WidgetGeometry.AbsolutePosition - FVector2D(ShadowBrush->Margin.Left, ShadowBrush->Margin.Top) * ShadowBrush->ImageSize * WidgetGeometry.Scale * TutorialConstants::ShadowScale),
										((WidgetGeometry.Size * WidgetGeometry.Scale) + (FVector2D(ShadowBrush->Margin.Right * 2.0f, ShadowBrush->Margin.Bottom * 2.0f) * ShadowBrush->ImageSize * WidgetGeometry.Scale * TutorialConstants::ShadowScale)),
										WidgetGeometry.Scale * TutorialConstants::ShadowScale);
		// draw highlight shadow
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, ShadowGeometry, ShadowBrush, WindowClippingRect, ESlateDrawEffect::None, ShadowTint);

		FVector2D PulseOffset = FVector2D(PulseFactor * TutorialConstants::MaxBorderOffset, PulseFactor * TutorialConstants::MaxBorderOffset);

		FVector2D BorderPosition = (WidgetGeometry.AbsolutePosition - ((FVector2D(BorderBrush->Margin.Left, BorderBrush->Margin.Top) * BorderBrush->ImageSize * WidgetGeometry.Scale) + PulseOffset));
		FVector2D BorderSize = ((WidgetGeometry.Size * WidgetGeometry.Scale) + (PulseOffset * 2.0f) + (FVector2D(BorderBrush->Margin.Right * 2.0f, BorderBrush->Margin.Bottom * 2.0f) * BorderBrush->ImageSize * WidgetGeometry.Scale));

		FPaintGeometry BorderGeometry(BorderPosition, BorderSize, WidgetGeometry.Scale);

		// draw highlight border
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId++, BorderGeometry, BorderBrush, WindowClippingRect, ESlateDrawEffect::None, BorderTint);
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

FReply STutorialContent::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (!bIsStandalone && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FSlateApplication::Get().PushMenu(AsShared(), HandleGetMenuContent(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

/** Helper function to generate title widget, if any */
static TSharedRef<SWidget> GetStageTitle(const FExcerpt& InExcerpt, int32 InCurrentExcerptIndex)
{
	// First try for unadorned 'StageTitle'
	FString VariableName(TEXT("StageTitle"));
	const FString* VariableValue = InExcerpt.Variables.Find(VariableName);
	if(VariableValue != NULL)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(*VariableValue))
			.TextStyle(FEditorStyle::Get(), "Tutorials.CurrentExcerpt");
	}

	// Then try 'StageTitle<StageNum>'
	VariableName = FString::Printf(TEXT("StageTitle%d"), InCurrentExcerptIndex + 1);
	VariableValue = InExcerpt.Variables.Find(VariableName);
	if(VariableValue != NULL)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(*VariableValue))
			.TextStyle(FEditorStyle::Get(), "Tutorials.CurrentExcerpt");
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> STutorialContent::GenerateContentWidget(const FTutorialContent& InContent, float WrapTextAt, TSharedPtr<IDocumentationPage>& OutDocumentationPage, const TAttribute<FText>& InHighlightText)
{
	// Style for the documentation
	static FDocumentationStyle DocumentationStyle;
	DocumentationStyle
		.ContentStyle(TEXT("Tutorials.Content.Text"))
		.BoldContentStyle(TEXT("Tutorials.Content.TextBold"))
		.NumberedContentStyle(TEXT("Tutorials.Content.Text"))
		.Header1Style(TEXT("Tutorials.Content.HeaderText1"))
		.Header2Style(TEXT("Tutorials.Content.HeaderText2"))
		.HyperlinkStyle(TEXT("Tutorials.Content.Hyperlink"))
		.HyperlinkTextStyle(TEXT("Tutorials.Content.HyperlinkText"))
		.SeparatorStyle(TEXT("Tutorials.Separator"));

	OutDocumentationPage = nullptr;

	switch(InContent.Type)
	{
	case ETutorialContent::Text:
		{
			return SNew(STextBlock)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.WrapTextAt(WrapTextAt)
				.Text(InContent.Text)
				.TextStyle(FEditorStyle::Get(), "Tutorials.Content")
				.HighlightText(InHighlightText)
				.HighlightColor(FEditorStyle::Get().GetColor("Tutorials.Browser.HighlightTextColor"));
		}

	case ETutorialContent::UDNExcerpt:
		if (IDocumentation::Get()->PageExists(InContent.Content))
		{
			OutDocumentationPage = IDocumentation::Get()->GetPage(InContent.Content, TSharedPtr<FParserConfiguration>(), DocumentationStyle);
			FExcerpt Excerpt;
			if(OutDocumentationPage->GetExcerpt(InContent.ExcerptName, Excerpt) && OutDocumentationPage->GetExcerptContent(Excerpt))
			{
				return SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 5.0f)
					[
						GetStageTitle(Excerpt, 0)
					]
					+SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.AutoHeight()
					[
						Excerpt.Content.ToSharedRef()
					];
			}
		}
		break;

	case ETutorialContent::RichText:
		{
			TArray< TSharedRef< class ITextDecorator > > Decorators;
			FTutorialText::GetRichTextDecorators(Decorators);

			return SNew(SRichTextBlock)
					.Visibility(EVisibility::SelfHitTestInvisible)
					.TextStyle(FEditorStyle::Get(), "Tutorials.Content.Text")
					.DecoratorStyleSet(&FEditorStyle::Get())
					.Decorators(Decorators)
					.Text(InContent.Text)
					.WrapTextAt(WrapTextAt)
					.Margin(4)
					.LineHeightPercentage(1.1f)
					.HighlightText(InHighlightText);
		}
		break;
	}

	return SNullWidget::NullWidget;
}


FVector2D STutorialContent::GetPosition() const
{
	bool bNonVisibleWidgetBound = bAllowNonWidgetContent && !bIsVisible && Anchor.Type == ETutorialAnchorIdentifier::NamedWidget;
	if(bNonVisibleWidgetBound)
	{
		if(OnWasWidgetDrawn.IsBound())
		{
			bNonVisibleWidgetBound &= !OnWasWidgetDrawn.Execute(Anchor.WrapperIdentifier);
		}
	}

	if(bNonVisibleWidgetBound)
	{
		// fallback: center on cached window
		return FVector2D((CachedWindowSize.X * 0.5f) - (ContentWidget->GetDesiredSize().X * 0.5f),
						 (CachedWindowSize.Y * 0.5f) - (ContentWidget->GetDesiredSize().Y * 0.5f));
	}
	else
	{
		float XOffset = 0.0f;
		switch(HorizontalAlignment.Get())
		{
		case HAlign_Left:
			XOffset = -(ContentWidget->GetDesiredSize().X - ContentOffset);
			break;
		default:
		case HAlign_Fill:
		case HAlign_Center:
			XOffset = (CachedGeometry.Size.X * 0.5f) - (ContentWidget->GetDesiredSize().X * 0.5f);
			break;
		case HAlign_Right:
			XOffset = CachedGeometry.Size.X - ContentOffset;
			break;
		}

		XOffset += WidgetOffset.Get().X;

		float YOffset = 0.0f;
		switch(VerticalAlignment.Get())
		{
		case VAlign_Top:
			YOffset = -(ContentWidget->GetDesiredSize().Y - ContentOffset);
			break;
		default:
		case VAlign_Fill:
		case VAlign_Center:
			YOffset = (CachedGeometry.Size.Y * 0.5f) - (ContentWidget->GetDesiredSize().Y * 0.5f);
			break;
		case VAlign_Bottom:
			YOffset = (CachedGeometry.Size.Y - ContentOffset);
			break;
		}

		YOffset += WidgetOffset.Get().Y;

		// now build & clamp to area
		FVector2D BaseOffset = FVector2D(CachedGeometry.AbsolutePosition.X + XOffset, CachedGeometry.AbsolutePosition.Y + YOffset);
		BaseOffset.X = FMath::Clamp(BaseOffset.X, 0.0f, CachedWindowSize.X - ContentWidget->GetDesiredSize().X);
		BaseOffset.Y = FMath::Clamp(BaseOffset.Y, 0.0f, CachedWindowSize.Y - ContentWidget->GetDesiredSize().Y);
		return BaseOffset;
	}
}

FVector2D STutorialContent::GetSize() const
{
	return ContentWidget->GetDesiredSize();
}

FReply STutorialContent::OnCloseButtonClicked()
{
	OnClosed.ExecuteIfBound();

	return FReply::Handled();
}

EVisibility STutorialContent::GetCloseButtonVisibility() const
{
	return bIsStandalone ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility STutorialContent::GetMenuButtonVisibility() const
{
	return !bIsStandalone ? EVisibility::Visible : EVisibility::Collapsed;
}

void STutorialContent::HandlePaintNamedWidget(TSharedRef<SWidget> InWidget, const FGeometry& InGeometry)
{
	switch(Anchor.Type)
	{
	case ETutorialAnchorIdentifier::NamedWidget:
		{
			TSharedPtr<FTagMetaData> MetaData = InWidget->GetMetaData<FTagMetaData>();
			if( Anchor.WrapperIdentifier == InWidget->GetTag() ||
				(MetaData.IsValid() && MetaData->Tag == Anchor.WrapperIdentifier))
			{
				bIsVisible = true;
				CachedGeometry = InGeometry;
			}
		}
		break;
	}
}

void STutorialContent::HandleResetNamedWidget()
{
	bIsVisible = false;
}

void STutorialContent::HandleCacheWindowSize(const FVector2D& InWindowSize)
{
	CachedWindowSize = InWindowSize;
}

EVisibility STutorialContent::GetVisibility() const
{
	const bool bVisibleWidgetBound = bIsVisible && Anchor.Type == ETutorialAnchorIdentifier::NamedWidget;
	const bool bNonWidgetBound =  Anchor.Type == ETutorialAnchorIdentifier::None;

	// fallback if widget is not drawn - we should display this content anyway
	bool bNonVisibleWidgetBound = bAllowNonWidgetContent && !bIsVisible && Anchor.Type == ETutorialAnchorIdentifier::NamedWidget;
	if(bNonVisibleWidgetBound)
	{
		if(OnWasWidgetDrawn.IsBound())
		{
			bNonVisibleWidgetBound &= !OnWasWidgetDrawn.Execute(Anchor.WrapperIdentifier);
		}	
	}

	return (bVisibleWidgetBound || bNonWidgetBound || bNonVisibleWidgetBound) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

FSlateColor STutorialContent::GetBackgroundColor() const
{
	// note cant use IsHovered() here because our widget is SelfHitTestInvisible
	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
	return CachedContentGeometry.IsUnderLocation(CursorPos) ? FEditorStyle::Get().GetColor("Tutorials.Content.Color.Hovered") : FEditorStyle::Get().GetColor("Tutorials.Content.Color");
}

float STutorialContent::GetAnimatedZoom() const
{
	if(ContentIntroAnimation.IsPlaying() && FSlateApplication::Get().IsRunningAtTargetFrameRate())
	{
		FIntroTutorials& IntroTutorials = FModuleManager::GetModuleChecked<FIntroTutorials>(TEXT("IntroTutorials"));
		return 0.75f + (0.25f * IntroTutorials.GetIntroCurveValue(ContentIntroAnimation.GetLerp()));
	}
	else
	{
		return 1.0f;
	}
}

float STutorialContent::GetInverseAnimatedZoom() const
{
	return 1.0f / GetAnimatedZoom();
}

TSharedRef<SWidget> STutorialContent::HandleGetMenuContent()
{
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, TSharedPtr<const FUICommandList>());

	MenuBuilder.BeginSection(TEXT("Tutorial Options"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExitLabel", "Exit"),
			LOCTEXT("ExitTooltip", "Quit this tutorial. You can find it again in the tutorials browser, reached from the Help menu."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &STutorialContent::HandleExitSelected),
				FCanExecuteAction()
			)
		);

		if(IsNextEnabled.Get())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("NextLabel", "Next"),
				GetNextButtonTooltip(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &STutorialContent::HandleNextSelected),
					FCanExecuteAction()
				)
			);
		}

		if(IsBackEnabled.Get())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("BackLabel", "Back"),
				LOCTEXT("BackTooltip", "Go back to the previous stage of this tutorial."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &STutorialContent::HandleBackSelected),
					FCanExecuteAction()
				)
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RestartLabel", "Restart"),
				LOCTEXT("RestartTooltip", "Start this tutorial again from the beginning."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &STutorialContent::HandleRestartSelected),
					FCanExecuteAction()
				)
			);
		}

		if(IsHomeEnabled.Get())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("OpenBrowserLabel", "More Tutorials..."),
				LOCTEXT("OpenBrowserTooltip", "Open the tutorial browser to find more tutorials."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &STutorialContent::HandleBrowseSelected),
					FCanExecuteAction()
				)
			);	
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void STutorialContent::HandleExitSelected()
{
	OnClosed.ExecuteIfBound();
}

void STutorialContent::HandleNextSelected()
{
	OnNextClicked.ExecuteIfBound();
}

void STutorialContent::HandleBackSelected()
{
	OnBackClicked.ExecuteIfBound();
}

void STutorialContent::HandleRestartSelected()
{
	if(Tutorial.IsValid())
	{
		FIntroTutorials& IntroTutorials = FModuleManager::GetModuleChecked<FIntroTutorials>(TEXT("IntroTutorials"));
		const bool bRestart = true;
		IntroTutorials.LaunchTutorial(Tutorial.Get(), bRestart, FSlateApplication::Get().FindWidgetWindow(AsShared()));

		if( FEngineAnalytics::IsAvailable() )
		{
			FEngineAnalytics::GetProvider().RecordEvent( FIntroTutorials::AnalyticsEventNameFromTutorial(TEXT("Rocket.Tutorials.Restarted"), Tutorial.Get()) );
		}
	}
}

void STutorialContent::HandleBrowseSelected()
{
	if( FEngineAnalytics::IsAvailable() && Tutorial.IsValid())
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("FromTutorial"), FIntroTutorials::AnalyticsEventNameFromTutorial(TEXT(""), Tutorial.Get())));

		FEngineAnalytics::GetProvider().RecordEvent( TEXT("Rocket.Tutorials.OpenedBrowser"), EventAttributes );
	}

	OnHomeClicked.ExecuteIfBound();
}

FReply STutorialContent::HandleNextClicked()
{
	if(IsNextEnabled.Get())
	{
		OnNextClicked.ExecuteIfBound();
	}
	else
	{
		OnHomeClicked.ExecuteIfBound();
	}
	
	return FReply::Handled();
}

const FSlateBrush* STutorialContent::GetNextButtonBrush() const
{
	if(IsNextEnabled.Get())
	{
		return FEditorStyle::GetBrush("Tutorials.Navigation.NextButton");
	}
	else
	{
		return FEditorStyle::GetBrush("Tutorials.Navigation.HomeButton");
	}
}

FText STutorialContent::GetNextButtonTooltip() const
{
	if(IsNextEnabled.Get())
	{
		return LOCTEXT("NextButtonTooltip", "Go to the next stage of this tutorial.");
	}
	else
	{
		return LOCTEXT("HomeButtonTooltip", "This tutorial is complete. Open the tutorial browser to find more tutorials.");
	}
}

FSlateColor STutorialContent::GetNextButtonColor() const
{
	return NextButton->IsHovered() ? FLinearColor(0.1f, 0.1f, 0.1f, 1.0f) : FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

FReply STutorialContent::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Mouse back and forward buttons traverse history
	if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
	{
		if(IsBackEnabled.Get())
		{
			OnBackClicked.ExecuteIfBound();
			return FReply::Handled();
		}
	}
	else if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
	{
		if(IsNextEnabled.Get())
		{
			OnNextClicked.ExecuteIfBound();
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply STutorialContent::OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent )
{
	// Mouse back and forward buttons traverse history
	if ( InMouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
	{
		if(IsBackEnabled.Get())
		{
			OnBackClicked.ExecuteIfBound();
			return FReply::Handled();
		}
	}
	else if ( InMouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
	{
		if(IsNextEnabled.Get())
		{
			OnNextClicked.ExecuteIfBound();
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE