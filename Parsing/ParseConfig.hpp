/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ParseConfig.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: marvin <marvin@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/17 20:26:54 by marvin            #+#    #+#             */
/*   Updated: 2026/03/17 20:26:54 by marvin           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef PARSECONFIG_HPP
#define PARSECONFIG_HPP

#include <fstream>
#include <vector>
#include "ServerConfig.hpp"

std::vector<ServerConfig> parseConfig(std::string path);

#endif