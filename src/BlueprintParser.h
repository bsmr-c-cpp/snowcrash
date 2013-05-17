//
//  BlueprintParser.h
//  snowcrash
//
//  Created by Zdenek Nemec on 4/16/13.
//  Copyright (c) 2013 Apiary.io. All rights reserved.
//

#ifndef SNOWCRASH_BLUEPRINTPARSER_H
#define SNOWCRASH_BLUEPRINTPARSER_H

#include <functional>
#include "BlueprintParserCore.h"
#include "Blueprint.h"
#include "ResourceParser.h"
#include "ResourceGroupParser.h"

namespace snowcrash {
    
    //
    // Block Classifier, Resource Context
    //
    template <>
    inline Section TClassifyBlock<Blueprint>(const BlockIterator& begin,
                                             const BlockIterator& end,
                                             const Section& context) {
        
        if (begin->type == HRuleBlockType)
            return TerminatorSection;
        
        if (HasResourceSignature(*begin))
            return ResourceGroupSection; // Treat Resource as anonymous resource group
        
        if (context == ResourceGroupSection ||
            context == TerminatorSection)
            return ResourceGroupSection;
        
        return BlueprintSection;
    }
    
    //
    // Blueprint Section Overview Parser
    //
    template<>
    struct SectionOverviewParser<Blueprint>  {
        
        static ParseSectionResult ParseSection(const Section& section,
                                               const BlockIterator& cur,
                                               const SectionBounds& bounds,
                                               const SourceData& sourceData,
                                               const Blueprint& blueprint,
                                               Blueprint& output) {
            if (section != BlueprintSection)
                return std::make_pair(Result(), cur);
            
            Result result;
            BlockIterator sectionCur(cur);
            if (sectionCur->type == HeaderBlockType &&
                sectionCur == bounds.first) {
                output.name = cur->content;
            }
            else {
                if (sectionCur == bounds.first) {
                    
                    // WARN: No API name specified
                    result.warnings.push_back(Warning("expected API name",
                                                      0,
                                                      cur->sourceMap));
                }
                

                if (sectionCur->type == QuoteBlockBeginType) {
                    sectionCur = SkipToSectionEnd(cur, bounds.second, QuoteBlockBeginType, QuoteBlockEndType);
                }
                else if (sectionCur->type == ListBlockBeginType) {
                    sectionCur = SkipToSectionEnd(cur, bounds.second, ListBlockBeginType, ListBlockEndType);
                }
                
                output.description += MapSourceData(sourceData, sectionCur->sourceMap);
            }
            
            return std::make_pair(result, ++sectionCur);
        }
    };
    
    typedef BlockParser<Blueprint, SectionOverviewParser<Blueprint> > BlueprintOverviewParser;
    
    //
    // Blueprint Section Parser
    //
    template<>
    struct SectionParser<Blueprint> {
        
        static ParseSectionResult ParseSection(const Section& section,
                                               const BlockIterator& cur,
                                               const SectionBounds& bounds,
                                               const SourceData& sourceData,
                                               const Blueprint& blueprint,
                                               Blueprint& output) {
            
            ParseSectionResult result = std::make_pair(Result(), cur);
            
            switch (section) {
                case TerminatorSection:
                    result.second = ++BlockIterator(cur);
                    break;
                    
                case BlueprintSection:
                    result = BlueprintOverviewParser::Parse(cur, bounds.second, sourceData, blueprint, output);
                    break;
                    
                case ResourceGroupSection:
                    result = HandleResourceGroup(cur, bounds.second, sourceData, blueprint, output);
                    break;
                    
                default:
                    result.first.error = Error("unexpected block", 1, cur->sourceMap);
                    break;
            }
            
            return result;
        }
        
        static ParseSectionResult HandleResourceGroup(const BlockIterator& begin,
                                                      const BlockIterator& end,
                                                      const SourceData& sourceData,
                                                      const Blueprint& blueprint,
                                                      Blueprint& output)
        {
            ResourceGroup resourceGroup;
            ParseSectionResult result = ResourceGroupParser::Parse(begin, end, sourceData, blueprint, resourceGroup);
            if (result.first.error.code != Error::OK)
                return result;
            
            Collection<ResourceGroup>::const_iterator duplicate = FindResourceGroup(blueprint, resourceGroup);
            if (duplicate != blueprint.resourceGroups.end()) {
                
                // WARN: duplicate group
                result.first.warnings.push_back(Warning("group '" +
                                                        resourceGroup.name +
                                                        "' already exists",
                                                        0,
                                                        begin->sourceMap));
            }
            
            output.resourceGroups.push_back(resourceGroup); // FIXME: C++11 move
            return result;
        }
        
    };
    
    typedef BlockParser<Blueprint, SectionParser<Blueprint> > BlueprintParserInner;
    
    
    //
    // Blueprint Parser
    //
    class BlueprintParser {
    public:
        // Parse Markdown AST into API Blueprint AST
        static void Parse(const SourceData& sourceData,
                          const MarkdownBlock::Stack& source,
                          Result& result,
                          Blueprint& blueprint) {
            
            ParseSectionResult sectionResult = BlueprintParserInner::Parse(source.begin(),
                                                                           source.end(),
                                                                           sourceData,
                                                                           blueprint,
                                                                           blueprint);
            result += sectionResult.first;
        }
    };
}

#endif
